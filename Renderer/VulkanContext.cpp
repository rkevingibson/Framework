#include "VulkanContext.h"
#include <algorithm>
#include <bitset>
#include <cstdint>
#include <numeric>
#include <unordered_set>
#define GLFW_INCLUDE_VULKAN
#include <External/GLFW/glfw3.h>
#include <Utilities/Utilities.h>
#include <vulkan/vulkan.hpp>


using namespace rkg::render;


#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))
#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)
#define FILE_LOCATION __FILE__ ":" TO_STRING(__LINE__)
#define CONCAT_STRING(x, y) (x##y)

#define CONSTRUCT_ERROR_STRING(x) ("Error: " FILE_LOCATION ", " x)

#define VK_FAIL_FAST(x, ...)                                                                       \
	if (!(x)) {                                                                                    \
		throw std::runtime_error(CONSTRUCT_ERROR_STRING(__VA_ARGS__));                             \
	}

#define VK_CHECK_RESULT(x, ...)                                                                    \
	if ((x) != VK_SUCCESS) {                                                                       \
		throw std::runtime_error(CONSTRUCT_ERROR_STRING(__VA_ARGS__));                             \
	}

namespace {
/*
	Want to start as small as possible with this:
	Just be able to draw a triangle.
	In order to do this:
	* Create shaders
	* Create a pipeline
	* Submit draws.
	* Synchronization primitives.

	Ugh that's a lot of work. I guess start with just the devices.
	How to store state? Right now, I'm kind of assuming it's stored in the RenderContext.
	But I'm not using virtual functions, so I'm either going to need to pass
	it explicitly, or assume I have global state and just store it statically here.
	Neither is a good option, really. Global state is bad, though not as bad since it's
	file-level. Having to pass it explicitly just makes for ugly code, full of casting.
	The Renderdoc API doesn't require the "this" pointer style, so it must have state
	held somewhere. That's definitely what I'm leaning towards right now.
*/
RenderContext vulkan_context;

VkBool32 debug_callback_fn(VkDebugReportFlagsEXT      flags,
						   VkDebugReportObjectTypeEXT object_type,
						   uint64_t                   object,
						   size_t                     location,
						   int32_t                    message_code,
						   const char*                layer_prefix,
						   const char*                message,
						   void*                      user_data)
{
	printf("%s: %s", layer_prefix, message);
	return true;
}
}  // namespace

using namespace rkg::render;

RenderContext* rkg::render::GetVulkanRenderContext()
{
	return &vulkan_context;
}

#pragma region               Initialization
std::vector<ValidationLayer> RenderContext::GetAvailableValidationLayers()
{
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	std::vector<VkLayerProperties> vk_layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, vk_layers.data());

	std::vector<ValidationLayer> layers(layer_count);

	for (uint32_t i = 0; i < layer_count; ++i) {
		memcpy(layers[i].name, vk_layers[i].layerName, SHORT_STRING_LENGTH);
		memcpy(layers[i].description, vk_layers[i].description, SHORT_STRING_LENGTH);
		layers[i].spec_version = vk_layers[i].specVersion;
		layers[i].version      = vk_layers[i].implementationVersion;
	}
	return layers;
}

std::vector<Extension> RenderContext::GetAvailableInstanceExtensions(
	const char* layer_name = nullptr)
{
	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(layer_name, &extension_count, nullptr);
	std::vector<VkExtensionProperties> vk_extensions(extension_count);
	vkEnumerateInstanceExtensionProperties(layer_name, &extension_count, vk_extensions.data());

	std::vector<Extension> extensions(extension_count);
	for (uint32_t i = 0; i < extension_count; ++i) {
		memcpy(extensions[i].name, vk_extensions[i].extensionName, SHORT_STRING_LENGTH);
		extensions[i].spec_version = vk_extensions[i].specVersion;
	}

	return extensions;
}

void RenderContext::CreateInstance(const CreateInstanceInfo& info)
{
	{
		VkApplicationInfo appInfo  = {};
		appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName   = info.application_name;
		appInfo.applicationVersion = info.application_version;
		appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
		appInfo.pEngineName        = "No Engine";
		appInfo.apiVersion         = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo    = {};
		createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo        = &appInfo;
		createInfo.ppEnabledExtensionNames = info.extension_names;
		createInfo.enabledExtensionCount   = info.extension_count;
		createInfo.ppEnabledLayerNames     = info.validation_layers;
		createInfo.enabledLayerCount       = info.validation_layer_count;

		VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &_instance),
						"Failed to create instance");
	}

	{
		VkDebugReportCallbackCreateInfoEXT debug_info = {};
		debug_info.sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		debug_info.flags       = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		debug_info.pfnCallback = debug_callback_fn;

		auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
			_instance, "vkCreateDebugReportCallbackEXT");
		VkResult result;
		if (func != nullptr) {
			result = func(_instance, &debug_info, nullptr, &_debug_callback_obj);
		}
		else {
			result = VK_ERROR_EXTENSION_NOT_PRESENT;
		}
		VK_CHECK_RESULT(result, "Failed to create debug callback");
	}


	VK_CHECK_RESULT(glfwCreateWindowSurface(_instance, info.glfw_window, nullptr, &_surface),
					"GLFW failed to create window surface.");
	_window = info.glfw_window;
}

namespace {
std::vector<QueueFamily> GetQueueFamilies(VkPhysicalDevice device)
{
	std::vector<QueueFamily> result;
	uint32_t                 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	result.reserve(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

	for (auto& queue_family : queue_families) {
		QueueFamily qf;
		qf.capability           = queue_family.queueFlags;
		qf.queue_count          = queue_family.queueCount;
		qf.timestamp_valid_bits = queue_family.timestampValidBits;
		result.emplace_back(std::move(qf));
	}
	return result;
}


}  // namespace

std::vector<PhysicalDevice> RenderContext::GetPhysicalDevices()
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);
	VK_FAIL_FAST(device_count != 0, "Failed to find a gpu with vulkan support");

	std::vector<VkPhysicalDevice> vk_devices(device_count);
	vkEnumeratePhysicalDevices(_instance, &device_count, vk_devices.data());

	std::vector<PhysicalDevice> devices;
	for (const auto& vk_device : vk_devices) {
		PhysicalDevice physical_device;
		physical_device.handle = reinterpret_cast<PhysicalDeviceHandle>(vk_device);
		// Basic Properties
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(vk_device, &properties);
			physical_device.api_version    = properties.apiVersion;
			physical_device.device_id      = properties.deviceID;
			physical_device.driver_version = properties.driverVersion;
			physical_device.vendor_id      = properties.vendorID;
		}

		// Queue Families
		{
			physical_device.queue_families = GetQueueFamilies(vk_device);
		}

		// Device Extensions
		{
			uint32_t extension_count = 0;
			vkEnumerateDeviceExtensionProperties(vk_device, nullptr, &extension_count, nullptr);
			std::vector<VkExtensionProperties> available_extensions(extension_count);
			vkEnumerateDeviceExtensionProperties(
				vk_device, nullptr, &extension_count, available_extensions.data());

			for (const auto& e : available_extensions) {
				Extension extension;
				memcpy(extension.name, e.extensionName, sizeof(e.extensionName));
				extension.spec_version = e.specVersion;
				physical_device.extensions.emplace_back(std::move(extension));
			}
		}

		// Device memory
		{
			VkPhysicalDeviceMemoryProperties memory_properties;
			vkGetPhysicalDeviceMemoryProperties(vk_device, &memory_properties);
			physical_device.heaps.resize(memory_properties.memoryHeapCount);
			for (uint32_t ii = 0; ii < memory_properties.memoryHeapCount; ++ii) {
				physical_device.heaps[ii].size = memory_properties.memoryHeaps[ii].size;
			}

			for (uint32_t ii = 0; ii < memory_properties.memoryTypeCount; ++ii) {
				auto& type = memory_properties.memoryTypes[ii];
				physical_device.heaps[type.heapIndex].types.emplace_back(type.propertyFlags);
			}
		}

		devices.emplace_back(std::move(physical_device));
	}
	return devices;
}

std::vector<Queue> RenderContext::CreateLogicalDevice(const CreateLogicalDeviceInfo& info)
{
	// Create queues, as needed.

	size_t                               num_queue_creates = info.queue_create_infos.size();
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos(num_queue_creates);
	for (size_t i = 0; i < num_queue_creates; ++i) {
		queue_create_infos[i].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_infos[i].pNext            = nullptr;
		queue_create_infos[i].flags            = 0;
		queue_create_infos[i].pQueuePriorities = &info.queue_create_infos[i].priority;
		queue_create_infos[i].queueCount       = info.queue_create_infos[i].num_queues;
		queue_create_infos[i].queueFamilyIndex = info.queue_create_infos[i].queue_family_index;
	}
	// Create a logical device
	{
		VkDeviceCreateInfo create_info = {};
		create_info.sType              = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		// TODO: Need to create some queues
		create_info.pQueueCreateInfos    = queue_create_infos.data();
		create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());

		VkPhysicalDeviceFeatures deviceFeatures = {};
		create_info.pEnabledFeatures            = &deviceFeatures;

		create_info.enabledExtensionCount   = info.extension_count;
		create_info.ppEnabledExtensionNames = info.extension_names;

		create_info.enabledLayerCount   = info.validation_layer_count;
		create_info.ppEnabledLayerNames = info.validation_layers;
		_physical_device                = reinterpret_cast<VkPhysicalDevice>(info.physical_device);
		_queue_families                 = GetQueueFamilies(_physical_device);
		vkCreateDevice(_physical_device, &create_info, nullptr, &_device);
	}

	// Get the queues.
	// For now, we just let the user specify what they need. Hard to figure out a general purpose
	// approach to this.
	std::vector<Queue> queues;
	uint32_t           queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(
		reinterpret_cast<VkPhysicalDevice>(info.physical_device), &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(
		reinterpret_cast<VkPhysicalDevice>(info.physical_device),
		&queue_family_count,
		queue_families.data());

	for (size_t i = 0; i < num_queue_creates; ++i) {
		auto& queue_create = info.queue_create_infos[i];
		auto  capability   = queue_families[queue_create.queue_family_index].queueFlags;
		for (uint32_t queue_i = 0; queue_i < queue_create.num_queues; ++queue_i) {
			VkQueue queue;
			vkGetDeviceQueue(_device, queue_create.queue_family_index, queue_i, &queue);
			queues.emplace_back(Queue{reinterpret_cast<QueueHandle>(queue), capability});
		}
	}

	return queues;
}

namespace {
struct SwapchainSupportDetails {
	VkSurfaceCapabilitiesKHR        capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR>   present_modes;
};

SwapchainSupportDetails GetSwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapchainSupportDetails result{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result.capabilities);


	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
	if (format_count != 0) {
		result.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, result.formats.data());
	}

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
	if (present_mode_count != 0) {
		result.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			device, surface, &present_mode_count, result.present_modes.data());
	}
	return result;
}

}  // namespace
void RenderContext::CreateImageViews()
{
	_swapchain_image_views.resize(_swapchain_images.size());
	const size_t num_images = _swapchain_images.size();
	for (size_t i = 0; i < num_images; ++i) {
		VkImageViewCreateInfo create_info{};
		create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image                           = _swapchain_images[i];
		create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format                          = _swapchain_image_format;
		create_info.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel   = 0;
		create_info.subresourceRange.levelCount     = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount     = 1;

		VK_CHECK_RESULT(
			vkCreateImageView(_device, &create_info, nullptr, &_swapchain_image_views[i]),
			"Failed to create an image view");
	}
}

namespace {
VkAccessFlags GetVkAccessFlags(AttachmentWriteFlags write, AttachmentReadFlags read)
{
	// NOTE: These lists have to stay in the same order as the enums in RenderContext.h.
	// TODO: Write a unit test to check that these stay in the same order.

	constexpr static VkAccessFlagBits write_map[] = {VK_ACCESS_SHADER_WRITE_BIT,
													 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
													 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
													 VK_ACCESS_TRANSFER_WRITE_BIT,
													 VK_ACCESS_HOST_WRITE_BIT,
													 VK_ACCESS_MEMORY_WRITE_BIT,
													 VK_ACCESS_COMMAND_PROCESS_WRITE_BIT_NVX};

	constexpr static VkAccessFlagBits read_map[] = {VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
													VK_ACCESS_INDEX_READ_BIT,
													VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
													VK_ACCESS_UNIFORM_READ_BIT,
													VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
													VK_ACCESS_SHADER_READ_BIT,
													VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
													VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
													VK_ACCESS_TRANSFER_READ_BIT,
													VK_ACCESS_HOST_READ_BIT,
													VK_ACCESS_MEMORY_READ_BIT,
													VK_ACCESS_COMMAND_PROCESS_READ_BIT_NVX};
	VkAccessFlags                     flags      = 0;
	for (uint32_t i = 0; i < 8 * sizeof(AttachmentWriteFlags); ++i, write >>= 1) {
		if (write & 1) {
			flags |= write_map[i];
		}
	}
	for (uint32_t i = 0; i < 8 * sizeof(AttachmentReadFlags); ++i, read >>= 1) {
		if (read & 1) {
			flags |= read_map[i];
		}
	}

	return flags;
}
}  // namespace

VkExtent2D RenderContext::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetWindowSize(_window, &width, &height);

		VkExtent2D actual_extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
		actual_extent.width =
			std::max(capabilities.minImageExtent.width,
					 std::min(capabilities.maxImageExtent.width, actual_extent.width));
		actual_extent.height =
			std::max(capabilities.minImageExtent.height,
					 std::min(capabilities.maxImageExtent.height, actual_extent.height));

		return actual_extent;
	}
}

void RenderContext::CreateSwapchain(VkPresentModeKHR&            mode,
									uint32_t                     depth,
									const std::vector<uint32_t>& queue_family_indices)
{
	auto  swapchain_support = GetSwapchainSupport(_physical_device, _surface);
	auto& available_formats = swapchain_support.formats;
	// Select the best available format for the swapchain.
	// TODO: Right now, we only support 8 bit, 4 channel, SRGB color.
	VkSurfaceFormatKHR format = available_formats[0];
	{
		if (available_formats.size() == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {
			format = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
		}

		for (const auto& available_format : available_formats) {
			if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
				available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				format = available_format;
				break;
			}
		}
	}

	// Choose present mode. We try to get the one passed to this function, but have a preferred
	// fallback.
	VkPresentModeKHR present_mode;
	{
		VkPresentModeKHR           best_mode     = VK_PRESENT_MODE_FIFO_KHR;
		constexpr VkPresentModeKHR fallback_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		for (const auto& available_present_mode : swapchain_support.present_modes) {
			if (available_present_mode == mode) {
				best_mode = mode;
				break;
			}
			if (available_present_mode == fallback_mode) {
				best_mode = fallback_mode;
			}
		}

		present_mode = best_mode;
	}

	// Choose the swapchain extent.
	VkExtent2D extent = ChooseSwapchainExtent(swapchain_support.capabilities);

	// TODO: Use the provided image count.
	uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
	if (swapchain_support.capabilities.maxImageCount > 0 &&
		image_count > swapchain_support.capabilities.maxImageCount) {
		image_count = swapchain_support.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface                  = _surface;
	create_info.minImageCount            = image_count;
	create_info.imageFormat              = format.format;
	create_info.imageColorSpace          = format.colorSpace;
	create_info.imageExtent              = extent;
	create_info.imageArrayLayers         = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if (queue_family_indices.size() > 1) {
		create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices   = queue_family_indices.data();
	}
	else {
		create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices   = nullptr;
	}

	create_info.preTransform   = swapchain_support.capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode    = present_mode;
	create_info.clipped        = VK_TRUE;
	create_info.oldSwapchain   = VK_NULL_HANDLE;

	for (auto queue_family : queue_family_indices) {
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(
			_physical_device, queue_family, _surface, &present_support);
	}


	VK_CHECK_RESULT(vkCreateSwapchainKHR(_device, &create_info, nullptr, &_swapchain),
					"Failed to create swapchain.");


	VK_CHECK_RESULT(vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, nullptr),
					"Failed to get swapchain image count.");
	_swapchain_images.resize(image_count);
	VK_CHECK_RESULT(
		vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, _swapchain_images.data()),
		"Failed to get swapchain images");


	_swapchain_image_format = format.format;

	CreateImageViews();
}

RenderPassHandle RenderContext::CreateRenderPass(const CreateRenderPassInfo& info)
{
	/*
	First, set up the subpasses. We need to know what subpasses reference which attachments, so
	that we can infer dependencies quickly.
	*/
	const uint32_t num_attachments = static_cast<uint32_t>(info.attachments.size());
	const uint32_t num_subpasses   = static_cast<uint32_t>(info.subpasses.size());

	struct AttachmentUsage {
		uint32_t             subpass_index;
		AttachmentWriteFlags write_flags{0};
		AttachmentReadFlags  read_flags{0};
		VkPipelineStageFlags access_stage;
	};

	std::vector<std::vector<AttachmentUsage>> attachment_usage(num_attachments);

	std::vector<VkSubpassDescription>               vk_subpasses;
	std::vector<std::vector<VkAttachmentReference>> color_refs(num_subpasses);
	std::vector<VkAttachmentReference>              depth_refs(num_subpasses);
	vk_subpasses.reserve(num_subpasses);

	for (uint32_t i = 0; i < num_subpasses; ++i) {
		const Subpass&        subpass    = info.subpasses[i];
		vk_subpasses.emplace_back();
		VkSubpassDescription& vk_subpass = vk_subpasses.back();

		color_refs[i].reserve(subpass.color_attachments.size());
		for (auto& ref : subpass.color_attachments) {
			attachment_usage[ref.attachment_index].emplace_back(
				AttachmentUsage{i, ref.write_flags, ref.read_flags, ref.access_stage});
			color_refs[i].emplace_back(VkAttachmentReference{ref.attachment_index, ref.layout});
		}

		vk_subpass.pDepthStencilAttachment       = nullptr;
		VkAttachmentReference& depth_stencil_ref = depth_refs[i];
		if (subpass.depth_stencil_attachment.attachment_index != ~0u) {
			auto& ref = subpass.depth_stencil_attachment;
			attachment_usage[ref.attachment_index].emplace_back(AttachmentUsage{
				ref.attachment_index, ref.write_flags, ref.read_flags, ref.access_stage});
			depth_stencil_ref.attachment       = subpass.depth_stencil_attachment.attachment_index;
			depth_stencil_ref.layout           = subpass.depth_stencil_attachment.layout;
			vk_subpass.pDepthStencilAttachment = &depth_stencil_ref;
		}

		vk_subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
		vk_subpass.pColorAttachments    = color_refs[i].data();
	}

	// Analyze the dependencies created by the subpasses.
	std::vector<VkAttachmentDescription> vk_attachments;
	vk_attachments.reserve(num_attachments);
	std::vector<VkSubpassDependency> vk_dependencies;

	for (uint32_t i = 0; i < num_attachments; ++i) {
		const Attachment&        attachment    = info.attachments[i];
		vk_attachments.emplace_back();
		VkAttachmentDescription& vk_attachment = vk_attachments.back();
		vk_attachment.format                   = attachment.format;
		vk_attachment.samples =
			(VkSampleCountFlagBits)
				attachment.num_samples;  // HACK: Relying on the enum making sense. Which it does,
										 // according to the spec right now.
										 // Load op needs to be determined later on, unless load is
										 // specified here.
		vk_attachment.loadOp = (attachment.color_depth_flags & Attachment::ClearOnLoadBit)
								   ? VK_ATTACHMENT_LOAD_OP_CLEAR
								   : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		vk_attachment.storeOp = (attachment.color_depth_flags & Attachment::StoreBit)
									? VK_ATTACHMENT_STORE_OP_STORE
									: VK_ATTACHMENT_STORE_OP_DONT_CARE;
		vk_attachment.stencilLoadOp = (attachment.stencil_flags & Attachment::ClearOnLoadBit)
										  ? VK_ATTACHMENT_LOAD_OP_CLEAR
										  : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		vk_attachment.stencilStoreOp = (attachment.stencil_flags & Attachment::StoreBit)
										   ? VK_ATTACHMENT_STORE_OP_STORE
										   : VK_ATTACHMENT_STORE_OP_DONT_CARE;

		vk_attachment.initialLayout = attachment.initial_layout;
		vk_attachment.finalLayout   = attachment.final_layout;
		vk_attachment.flags         = 0;  // TODO: What are the flag possibilities;

		// Time for depedencies.
		uint32_t prev_read_op    = ~0u;
		uint32_t prev_write_op   = ~0u;
		auto&    attachment_refs = attachment_usage[i];

		for (uint32_t j = 0; j < attachment_refs.size(); ++j) {
			if (attachment_refs[j].read_flags != 0) {
				VkSubpassDependency dependency{};
				// TODO: Convert these values safely.
				dependency.dstAccessMask = GetVkAccessFlags(
					attachment_refs[j].write_flags,
					attachment_refs[j].read_flags);  // Combine read/write accesses here;
				dependency.dstSubpass   = attachment_refs[j].subpass_index;
				dependency.dstStageMask = attachment_refs[j].access_stage;

				if (prev_write_op == ~0u) {
					// No previous write, must have an external dependency.
					dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
					dependency.srcStageMask =
						VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;  // NOTE: This might be a
															   // pessimisation. Unclear how
															   // external dependencies work. This
															   // flag might be ignored completely.
					dependency.srcAccessMask = 0;
				}
				else {
					dependency.srcSubpass = attachment_refs[prev_write_op].subpass_index;
					dependency.srcAccessMask =
						GetVkAccessFlags(attachment_refs[prev_write_op].write_flags,
										 attachment_refs[prev_write_op].read_flags);
					dependency.srcStageMask = attachment_refs[prev_write_op].access_stage;
				}
				// TODO: dependency.dependencyFlags;
				vk_dependencies.emplace_back(std::move(dependency));
				prev_read_op = j;
			}
			if (attachment_refs[j].write_flags !=
				0) {  // If we're writing, do we have a dependency on prior reads? Not sure.

				prev_write_op = j;
			}
		}
	}


	VkRenderPassCreateInfo pass_info{};
	pass_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	pass_info.pNext           = nullptr;
	pass_info.attachmentCount = static_cast<uint32_t>(vk_attachments.size());
	pass_info.pAttachments    = vk_attachments.data();
	pass_info.subpassCount    = static_cast<uint32_t>(vk_subpasses.size());
	pass_info.pSubpasses      = vk_subpasses.data();
	pass_info.dependencyCount = static_cast<uint32_t>(vk_dependencies.size());
	pass_info.pDependencies   = vk_dependencies.data();
	VkRenderPass render_pass;
	VK_CHECK_RESULT(vkCreateRenderPass(_device, &pass_info, nullptr, &render_pass),
					"Failed to create render pass.");
	return reinterpret_cast<RenderPassHandle>(render_pass);
}  // namespace

#pragma endregion

void RenderContext::BindQueue(QueueHandle h)
{
	_current_queue = reinterpret_cast<VkQueue>(h);
}



VkBuffer RenderContext::CreateHostLocalBuffer(size_t             size,
											  VkBufferUsageFlags usage,
											  VkQueueFlags       queues)
{
	return VkBuffer();
}

VkBuffer RenderContext::CreateBuffer(size_t size, VkBufferUsageFlags usage, VkQueueFlags queues)
{
	VkMemoryAllocateInfo allocate_info{};
	allocate_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.allocationSize  = size;
	allocate_info.memoryTypeIndex = 0;
	VkDeviceMemory device_memory;
	vkAllocateMemory(_device, &allocate_info, nullptr, &device_memory);

	VkBufferCreateInfo buffer_info{};
	buffer_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
	buffer_info.usage       = usage;
	// TODO: Right now, just using all queue families. I assume this is terribly inefficient.
	std::vector<uint32_t> queue_families(_queue_families.size());
	std::iota(queue_families.begin(), queue_families.end(), 0);
	buffer_info.pQueueFamilyIndices   = queue_families.data();
	buffer_info.queueFamilyIndexCount = static_cast<uint32_t>(_queue_families.size());
	buffer_info.size                  = size;

	VkBuffer buffer;
	VK_CHECK_RESULT(vkCreateBuffer(_device, &buffer_info, nullptr, &buffer),
					"Failed to create buffer.");
	return buffer;
}

VkShaderModule RenderContext::CreateShaderModule(const std::vector<uint32_t>& code)
{
	VkShaderModuleCreateInfo info{};
	info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = code.size();
	info.pCode    = code.data();
	VkShaderModule shader;
	VK_CHECK_RESULT(vkCreateShaderModule(_device, &info, nullptr, &shader),
					"Failed to create shader module.");
	return shader;
}

VkPipeline RenderContext::CreateGraphicsPipeline(const VkGraphicsPipelineCreateInfo& info)
{
	VkPipeline pipeline;
	VK_CHECK_RESULT(
		vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline),
		"Failed to create graphics pipeline!");
	return pipeline;
}

bool GraphicsPipelineState::AddShader(VkShaderStageFlagBits stage,
									  VkShaderModule        module,
									  const char*           entry_point)
{
	for (auto& shader : _shaders) {
		if (shader.stage == stage) {
			return false;
		}
	}
	_shaders.emplace_back();
	auto info        = _shaders.back();
	info.stage       = stage;
	info.module      = module;
	info.entry_point = entry_point;
	return true;
}

bool GraphicsPipelineState::SetVertexInputState(
	const std::vector<VkVertexInputBindingDescription>&   bindings,
	const std::vector<VkVertexInputAttributeDescription>& attributes)
{
	if (_vertex_input != nullptr) {
		return false;
	}

	_vertex_input = std::make_unique<VertexInputInfo>(VertexInputInfo{bindings, attributes});
	return true;
}

bool GraphicsPipelineState::SetInputAssemblyState(VkPrimitiveTopology topology,
												  bool                primitive_restart)
{
	if (_input_assembly != nullptr) {
		return false;
	}

	_input_assembly = std::make_unique<VkPipelineInputAssemblyStateCreateInfo>(
		VkPipelineInputAssemblyStateCreateInfo{});
	_input_assembly->sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	_input_assembly->primitiveRestartEnable = primitive_restart;
	_input_assembly->topology               = topology;
	return true;
}

bool GraphicsPipelineState::SetTessellationState(uint32_t patch_control_points)
{
	if (_tessellation_info != nullptr) {
		return false;
	}

	_tessellation_info = std::make_unique<VkPipelineTessellationStateCreateInfo>(
		VkPipelineTessellationStateCreateInfo{});
	_tessellation_info->sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	_tessellation_info->patchControlPoints = patch_control_points;
	return true;
}

void GraphicsPipelineState::SetViewports(const std::vector<VkViewport>& viewports)
{
	VK_FAIL_FAST(viewports.size() == 0, "Overriding viewports");
	_viewports = viewports;
}

void GraphicsPipelineState::SetScissors(const std::vector<VkRect2D>& scissors)
{
	VK_FAIL_FAST(scissors.size() == 0, "Overriding scissors");
	_scissors = scissors;
}

void GraphicsPipelineState::SetColorBlendState(
	const std::vector<VkPipelineColorBlendAttachmentState>& attachment_state,
	float                                                   blend_constants[4])
{
	VK_FAIL_FAST(_color_blend_info == nullptr, "Overwriting previously set color blend state");

	_color_blend_info                     = std::make_unique<ColorBlendInfo>(ColorBlendInfo{});
	_color_blend_info->attachment_state   = attachment_state;
	_color_blend_info->blend_constants[0] = blend_constants[0];
	_color_blend_info->blend_constants[1] = blend_constants[1];
	_color_blend_info->blend_constants[2] = blend_constants[2];
	_color_blend_info->blend_constants[3] = blend_constants[3];
}

void GraphicsPipelineState::SetLogicOp(VkLogicOp op)
{
	VK_FAIL_FAST(_logic_op_enabled == false, "Overwriting previously set logic op.");

	_logic_op_enabled = true;
	_logic_op         = op;
}

void GraphicsPipelineState::SetDepthState(bool        depth_test,
										  bool        depth_write,
										  VkCompareOp op,
										  bool        depth_bounds_test,
										  float       min_depth_bounds,
										  float       max_depth_bounds)
{
	VK_FAIL_FAST(_depth_info == nullptr, "Overwriting depth state");
	_depth_info                            = std::make_unique<DepthInfo>(DepthInfo{});
	_depth_info->depth_bounds_test_enabled = depth_test;
	_depth_info->depth_write_enabled       = depth_write;
	_depth_info->op                        = op;
	_depth_info->depth_bounds_test_enabled = depth_bounds_test;
	_depth_info->min_depth                 = min_depth_bounds;
	_depth_info->max_depth                 = max_depth_bounds;
}

void GraphicsPipelineState::SetStencilState(bool             stencil_test,
											VkStencilOpState front,
											VkStencilOpState back)
{
	VK_FAIL_FAST(_stencil_info == nullptr, "Overwriting stencil state");
	_stencil_info          = std::make_unique<StencilInfo>(StencilInfo{});
	_stencil_info->enabled = stencil_test;
	_stencil_info->front   = front;
	_stencil_info->back    = back;
}

void GraphicsPipelineState::AddDynamicState(VkDynamicState state)
{
	_dynamic_state.push_back(state);
}

void GraphicsPipelineState::SetDepthBias(float depth_bias_constant,
										 float depth_bias_clamp,
										 float depth_bias_slope)
{
	VK_FAIL_FAST(_depth_bias == nullptr, "Overwriting depth bias!");
	_depth_bias           = std::make_unique<DepthBias>(DepthBias{});
	_depth_bias->constant = depth_bias_constant;
	_depth_bias->clamp    = depth_bias_clamp;
	_depth_bias->slope    = depth_bias_slope;
}

void GraphicsPipelineState::SetCullMode(VkFrontFace front_face, VkCullModeFlags cull_mode)
{
	VK_FAIL_FAST(_cull_mode == nullptr, "Cull mode being overwritten");
	_cull_mode             = std::make_unique<CullMode>();
	_cull_mode->cull_mode  = cull_mode;
	_cull_mode->front_face = front_face;
}

VkGraphicsPipelineCreateInfo GraphicsPipelineState::Compile()
{
	VK_FAIL_FAST(IsStateValid(), "Invalid pipeline state when compiling");
	VkGraphicsPipelineCreateInfo info{};

	std::unordered_set<VkDynamicState> dynamic_state_set(_dynamic_state.cbegin(),
														 _dynamic_state.cend());

	std::vector<VkPipelineShaderStageCreateInfo> shader_info;
	for (auto& shader : _shaders) {
		shader_info.emplace_back(VkPipelineShaderStageCreateInfo{});
		auto& s  = shader_info.back();
		s.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		s.module = shader.module;
		s.pName  = shader.entry_point.c_str();
		s.stage  = shader.stage;
	}
	info.pStages    = shader_info.data();
	info.stageCount = static_cast<uint32_t>(shader_info.size());

	VkPipelineVertexInputStateCreateInfo vertex_input_info{};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.pVertexAttributeDescriptions = _vertex_input->attributes.data();
	vertex_input_info.vertexAttributeDescriptionCount =
		static_cast<uint32_t>(_vertex_input->attributes.size());
	vertex_input_info.pVertexBindingDescriptions = _vertex_input->bindings.data();
	vertex_input_info.vertexBindingDescriptionCount =
		static_cast<uint32_t>(_vertex_input->bindings.size());
	info.pVertexInputState = &vertex_input_info;

	info.pInputAssemblyState = _input_assembly.get();
	info.pTessellationState  = _tessellation_info.get();

	VkPipelineViewportStateCreateInfo viewport_info{};
	viewport_info.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_info.viewportCount = static_cast<uint32_t>(_viewports.size());
	viewport_info.pViewports    = _viewports.data();
	viewport_info.scissorCount  = static_cast<uint32_t>(_scissors.size());
	viewport_info.pScissors     = _scissors.data();
	info.pViewportState         = &viewport_info;

	VkPipelineColorBlendStateCreateInfo color_blend_info{};
	color_blend_info.logicOpEnable = _logic_op_enabled;
	color_blend_info.logicOp       = _logic_op;
	color_blend_info.attachmentCount =
		static_cast<uint32_t>(_color_blend_info->attachment_state.size());
	color_blend_info.pAttachments = _color_blend_info->attachment_state.data();
	memcpy(color_blend_info.blendConstants,
		   _color_blend_info->blend_constants,
		   sizeof(color_blend_info.blendConstants));
	info.pColorBlendState = &color_blend_info;

	VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
	depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	if (_depth_info != nullptr) {
		depth_stencil_info.depthTestEnable       = _depth_info->depth_test_enabled;
		depth_stencil_info.depthBoundsTestEnable = _depth_info->depth_bounds_test_enabled;
		depth_stencil_info.depthWriteEnable      = _depth_info->depth_write_enabled;
		depth_stencil_info.depthCompareOp        = _depth_info->op;
		depth_stencil_info.minDepthBounds        = _depth_info->min_depth;
		depth_stencil_info.maxDepthBounds        = _depth_info->max_depth;
	}
	if (_stencil_info != nullptr) {
		depth_stencil_info.front             = _stencil_info->front;
		depth_stencil_info.back              = _stencil_info->back;
		depth_stencil_info.stencilTestEnable = _stencil_info->enabled;
	}
	info.pDepthStencilState = &depth_stencil_info;

	// TODO: Enable multisampling support
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable   = VK_FALSE;
	multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading      = 1.0f;
	multisampling.pSampleMask           = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable      = VK_FALSE;
	info.pMultisampleState              = &multisampling;

	VkPipelineDynamicStateCreateInfo dynamic_state{};
	dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pDynamicStates    = _dynamic_state.data();
	dynamic_state.dynamicStateCount = static_cast<uint32_t>(_dynamic_state.size());
	info.pDynamicState              = &dynamic_state;

	VkPipelineRasterizationStateCreateInfo rasterization_state{};
	rasterization_state.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.cullMode  = _cull_mode->cull_mode;
	rasterization_state.frontFace = _cull_mode->front_face;

	if (_depth_bias != nullptr) {
		rasterization_state.depthBiasEnable         = VK_TRUE;
		rasterization_state.depthBiasClamp          = _depth_bias->clamp;
		rasterization_state.depthBiasConstantFactor = _depth_bias->constant;
		rasterization_state.depthBiasSlopeFactor    = _depth_bias->slope;
	}
	else if (dynamic_state_set.count(VK_DYNAMIC_STATE_DEPTH_BIAS) != 0) {
		rasterization_state.depthBiasEnable = VK_TRUE;
	}

	if (dynamic_state_set.count(VK_DYNAMIC_STATE_LINE_WIDTH) == 0) {
		rasterization_state.lineWidth = _line_width;
	}

	rasterization_state.polygonMode             = _polygon_mode;
	rasterization_state.rasterizerDiscardEnable = _rasterizer_discard;
	rasterization_state.depthClampEnable        = _depth_clamp;
	info.pRasterizationState                    = &rasterization_state;

	//Can't do this - all the pointers will disappear. Need a better solution... boo.
	return info;
}

bool GraphicsPipelineState::IsStateValid()
{
	return true;
}
