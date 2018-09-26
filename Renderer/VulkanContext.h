#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <External/GLFW/glfw3.h>
#include <Utilities/Utilities.h>
#include <vulkan/vulkan.hpp>

namespace rkg {
namespace render {

using HandleType           = std::uintptr_t;
using PhysicalDeviceHandle = HandleType;
using QueueHandle          = HandleType;
using RenderPassHandle     = HandleType;
using TextureHandle        = HandleType;
using FramebufferHandle    = HandleType;
using BufferHandle         = HandleType;
using VertexBufferHandle   = HandleType;
using IndexBufferHandle    = HandleType;
using ProgramHandle        = HandleType;

static constexpr int SHORT_STRING_LENGTH = 256;

struct ValidationLayer {
	char     name[SHORT_STRING_LENGTH];
	char     description[SHORT_STRING_LENGTH];
	uint32_t version;
	uint32_t spec_version;
};

struct Extension {
	char     name[SHORT_STRING_LENGTH]{0};
	uint32_t spec_version{0};
};

struct CreateInstanceInfo {
	GLFWwindow* glfw_window;

	const char* application_name{""};
	uint32_t    application_version{0};

	uint32_t           extension_count{0};
	char const* const* extension_names{nullptr};

	uint32_t           validation_layer_count{0};
	char const* const* validation_layers{nullptr};

	// TODO: Backend-neutral debug callback.
	// PFN_vkDebugReportCallbackEXT debug_callback{nullptr};
};

struct QueueFamily {
	uint32_t     queue_count;
	uint32_t     timestamp_valid_bits;
	VkQueueFlags capability{0};
	// TODO: Do I care about minImageTransferGranularity?
};

struct MemoryHeap {
	size_t                             size;
	std::vector<VkMemoryPropertyFlags> types;
};

struct PhysicalDevice {
	PhysicalDeviceHandle handle;
	uint32_t             api_version;
	uint32_t             driver_version;
	uint32_t             vendor_id;
	uint32_t             device_id;
	// TODO: Device type enum.

	std::vector<QueueFamily> queue_families;
	std::vector<Extension>   extensions;
	std::vector<MemoryHeap>  heaps;
};

struct QueueCreateInfo {
	uint32_t queue_family_index;
	uint32_t num_queues{1};
	float    priority{1.f};
};

struct Queue {
	QueueHandle  handle;
	VkQueueFlags capabilities;
};

struct CreateLogicalDeviceInfo {
	PhysicalDeviceHandle         physical_device;
	std::vector<QueueCreateInfo> queue_create_infos;

	uint32_t           validation_layer_count{0};
	char const* const* validation_layers{nullptr};

	uint32_t           extension_count{0};
	char const* const* extension_names{nullptr};
};

struct Attachment {
	// Need to get an image format enum
	enum BehaviourFlags : uint8_t {
		ClearOnLoadBit = 1,
		StoreBit       = 2,
	};
	VkFormat      format;
	VkImageLayout initial_layout;
	VkImageLayout final_layout;
	uint8_t       num_samples;
	bool          may_alias;
	uint8_t       color_depth_flags{0};
	uint8_t       stencil_flags{0};
};

using AttachmentWriteFlags = uint16_t;
struct AttachmentWriteFlagBits {
	enum Enum : uint16_t {
		Shader                 = 1 << 0,
		ColorAttachment        = 1 << 1,
		DepthStencilAttachment = 1 << 2,
		Transfer               = 1 << 3,
		Host                   = 1 << 4,
		Memory                 = 1 << 5,
		CommandProcess         = 1 << 6,
	};
};

using AttachmentReadFlags = uint16_t;
struct AttachmentReadFlagBits {
	enum Enum : uint16_t {
		IndirectCommand        = 1 << 0,
		Index                  = 1 << 1,
		VertexAttribute        = 1 << 2,
		Uniform                = 1 << 3,
		InputAttachment        = 1 << 4,
		Shader                 = 1 << 5,
		ColorAttachment        = 1 << 6,
		DepthStencilAttachment = 1 << 7,
		Transfer               = 1 << 8,
		Host                   = 1 << 9,
		Memory                 = 1 << 10,
		CommandProcess         = 1 << 11,
	};
};

struct AttachmentRef {
	uint32_t             attachment_index{~0u};
	VkImageLayout        layout{VK_IMAGE_LAYOUT_UNDEFINED};
	VkPipelineStageFlags access_stage{0};
	AttachmentWriteFlags write_flags{0};
	AttachmentReadFlags  read_flags{0};
};

struct Subpass {
	enum class Type : uint8_t { Graphics, Compute };
	Type                       type{Type::Graphics};
	std::vector<AttachmentRef> color_attachments{};
	AttachmentRef              depth_stencil_attachment{};
};

struct CreateRenderPassInfo {
	std::vector<Attachment> attachments;
	std::vector<Subpass>    subpasses;
};

class RenderContext;

RenderContext* GetVulkanRenderContext();

class RenderContext {
   public:
	std::vector<ValidationLayer> GetAvailableValidationLayers();
	std::vector<Extension>       GetAvailableInstanceExtensions(const char* layer_name);
	void                         CreateInstance(const CreateInstanceInfo&);
	std::vector<Queue>           CreateLogicalDevice(const CreateLogicalDeviceInfo&);
	void                         BindQueue(QueueHandle);
	std::vector<PhysicalDevice>  GetPhysicalDevices();
	void                         CreateSwapchain(VkPresentModeKHR&            mode,
												 uint32_t                     depth,
												 const std::vector<uint32_t>& queue_family_indices);

	RenderPassHandle CreateRenderPass(const CreateRenderPassInfo& info);
	VkBuffer CreateHostLocalBuffer(size_t size, VkBufferUsageFlags usage, VkQueueFlags queues);
	VkBuffer CreateBuffer(size_t size, VkBufferUsageFlags usage, VkQueueFlags queues);
	VkBuffer CreateTemporaryBuffer(size_t size, VkBufferUsageFlags usage, VkQueueFlags queues);

	VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code);

	VkPipeline CreateGraphicsPipeline(const VkGraphicsPipelineCreateInfo& info);

   private:
	void       CreateImageViews();
	VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	VkInstance   _instance;
	VkSurfaceKHR _surface;

	VkDebugReportCallbackEXT _debug_callback_obj;

	VkDevice                 _device;
	VkPhysicalDevice         _physical_device;
	std::vector<QueueFamily> _queue_families;
	VkQueue                  _current_queue;
	VkSwapchainKHR           _swapchain;
	GLFWwindow*              _window;

	std::vector<VkImage>     _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;
	VkFormat                 _swapchain_image_format;
};

enum class Tristate : uint8_t { NotSet = 0, Disabled = 1, Enabled = 2 };

struct RasterizerState {
	enum PolygonMode_ { PolygonMode_Fill = 1, PolygonMode_Line, PolygonMode_Point };

	enum CullDirection_ { CullDirection_CCW = 1, CullDirection_CW };

	enum CullMode_ {
		CullMode_Disabled = 1,
		CullMode_FrontFace,
		CullMode_BackFace,
		CullMode_BothFaces
	};

	enum Scissor_ { Scissor_Disabled = 1, Scissor_Enabled, Scissor_Dynamic };

	uint8_t polygon_mode : 2;    // 0 = not set, 1 = GL_FILL, 2 = GL_LINE, 3 = GL_POINT.
	uint8_t cull_direction : 2;  // 0 = not set, 1 = ccw, 2 = cw;
	uint8_t cull_mode : 4;       // 0 = not set, 1 = no culling, 2 = front, 3 = back, 4 = both.
	uint8_t scissor_state : 2;   // 0 = not set, 1 = disabled, 2 = enabled, 3 = dynamic.
								 // TODO: How to store scissor box? How often is it used?
	int scissor_box[4];
};

struct DepthState {
	enum DepthFunc_ {
		DepthFunc_Never = 1,
		DepthFunc_Less,
		DepthFunc_Equal,
		DepthFunc_Lequal,
		DepthFunc_Greater,
		DepthFunc_NotEqual,
		DepthFunc_Gequal,
		DepthFunc_Always
	};

	Tristate depth_test : 2;   // 0 = not set, 1 = disabled, 2 = enabled;
	Tristate depth_write : 2;  // 0 = not set, 1 = disabled, 2 = enabled;
	uint8_t  depth_func : 4;
};

struct StencilState {
	// TODO: Stencil buffer state.

	// This needs a better name.
	Tristate enabled : 2;  // 0 = not set, 1 = enabled, 2 = disabled;
	uint8_t  front_function : 4;
	uint16_t front_operations;
};

struct BlendState {
	enum BlendFunc_ {
		BlendFunc_Zero = 1,
		BlendFunc_One,
		BlendFunc_SrcColor,
		BlendFunc_OneMinusSrcColor,
		BlendFunc_DstColor,
		BlendFunc_OneMinusDstColor,
		BlendFunc_SrcAlpha,
		BlendFunc_OneMinusSrcAlpha,
		BlendFunc_DstAlpha,
		BlendFunc_OneMinusDstAlpha,
		BlendFunc_ConstantColor,
		BlendFunc_OneMinusConstantColor,
		BlendFunc_ConstantAlpha,
		BlendFunc_OneMinusConstantAlpha,
		BlendFunc_SrcAlphaSaturate,
		BlendFunc_Src1Color,
		BlendFunc_OneMinusSrc1Color,
		BlendFunc_Src1Alpha,
		BlendFunc_OneMinusSrc1Alpha
	};

	enum BlendEq_ { BlendEq_Add = 1, BlendEq_Sub, BlendEq_ReverseSub, BlendEq_Min, BlendEq_Max };
	// TODO: Fix this, its terrible.
	uint8_t buffer;  // 0 - set for all buffers, otherwise set for buffer i-1. This could be
					 // potentially confusing.
	uint8_t func_rgb : 5;
	uint8_t eq_rgb : 3;
	uint8_t func_alpha : 5;
	uint8_t eq_alpha : 3;

	// TODO: Support for global BlendColor.
};

class GraphicsPipelineState {
   public:
	GraphicsPipelineState()                             = default;
	GraphicsPipelineState(const GraphicsPipelineState&) = default;
	GraphicsPipelineState(GraphicsPipelineState&&)      = default;
	GraphicsPipelineState& operator=(const GraphicsPipelineState&) = default;
	GraphicsPipelineState& operator=(GraphicsPipelineState&&) = default;

	bool AddShader(VkShaderStageFlagBits stage, VkShaderModule module, const char* entry_point);
	bool SetVertexInputState(const std::vector<VkVertexInputBindingDescription>&   bindings,
							 const std::vector<VkVertexInputAttributeDescription>& attributes);
	bool SetInputAssemblyState(VkPrimitiveTopology topology, bool primitive_restart);
	bool SetTessellationState(uint32_t patch_control_points);
	void SetViewports(const std::vector<VkViewport>& viewports);
	void SetScissors(const std::vector<VkRect2D>& scissors);
	void SetColorBlendState(
		const std::vector<VkPipelineColorBlendAttachmentState>& attachment_state,
		float                                                   blend_constants[4]);
	void SetLogicOp(VkLogicOp op);
	void SetDepthState(bool        depth_test,
					   bool        depth_write,
					   VkCompareOp op,
					   bool        depth_bounds_test,
					   float       min_depth_bounds = 0.0f,
					   float       max_depth_bounds = 1.0f);
	void SetStencilState(bool             stencil_test,
						 VkStencilOpState front = {},
						 VkStencilOpState back  = {});
	void AddDynamicState(VkDynamicState state);
	void SetDepthBias(float depth_bias_constant, float depth_bias_clamp, float depth_bias_slope);
	void SetCullMode(VkFrontFace front_face, VkCullModeFlags cull_mode);
	inline void SetLineWidth(float line_width) { _line_width = line_width; }
	inline void SetPolygonMode(VkPolygonMode mode) noexcept { _polygon_mode = mode; };
	inline void EnableRasterizerDiscard() noexcept { _rasterizer_discard = VK_TRUE; };
	inline void EnableDepthClamp() noexcept { _depth_clamp = VK_TRUE; };

	VkGraphicsPipelineCreateInfo Compile();

   private:
	struct ShaderModuleInfo {
		VkShaderStageFlagBits stage;
		VkShaderModule        module;
		std::string           entry_point;
	};

	struct VertexInputInfo {
		std::vector<VkVertexInputBindingDescription>   bindings;
		std::vector<VkVertexInputAttributeDescription> attributes;
	};

	struct ColorBlendInfo {
		std::vector<VkPipelineColorBlendAttachmentState> attachment_state;
		float                                            blend_constants[4];
	};

	struct DepthInfo {
		bool        depth_test_enabled;
		bool        depth_write_enabled;
		bool        depth_bounds_test_enabled;
		VkCompareOp op;
		float       min_depth;
		float       max_depth;
	};

	struct StencilInfo {
		bool             enabled;
		VkStencilOpState front;
		VkStencilOpState back;
	};

	struct CullMode {
		VkFrontFace     front_face;
		VkCullModeFlags cull_mode;
	};

	struct DepthBias {
		float constant;
		float clamp;
		float slope;
	};

	std::vector<ShaderModuleInfo>                           _shaders{};
	std::unique_ptr<VertexInputInfo>                        _vertex_input{nullptr};
	std::unique_ptr<VkPipelineInputAssemblyStateCreateInfo> _input_assembly{nullptr};
	std::unique_ptr<VkPipelineTessellationStateCreateInfo>  _tessellation_info{nullptr};
	std::vector<VkViewport>                                 _viewports{};
	std::vector<VkRect2D>                                   _scissors{};
	std::unique_ptr<ColorBlendInfo>                         _color_blend_info{nullptr};
	std::unique_ptr<DepthInfo>                              _depth_info{nullptr};
	std::unique_ptr<StencilInfo>                            _stencil_info{nullptr};
	std::unique_ptr<CullMode>                               _cull_mode{nullptr};
	std::unique_ptr<DepthBias>                              _depth_bias{nullptr};
	bool                                                    _logic_op_enabled{false};
	VkLogicOp                                               _logic_op;
	float                                                   _line_width{FLT_MAX};
	std::vector<VkDynamicState>                             _dynamic_state{};
	VkPolygonMode                                           _polygon_mode{VK_POLYGON_MODE_MAX_ENUM};
	bool                                                    _rasterizer_discard{VK_FALSE};
	bool                                                    _depth_clamp{VK_FALSE};

	bool IsStateValid();
};


}  // namespace render
}  // namespace rkg
