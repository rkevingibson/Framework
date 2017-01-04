#include "GuiBasics.h"
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define NOMINMAX

#pragma comment(lib, "glfw3.lib")
#endif
#include "../External/GLFW/glfw3native.h"
#include "../External/imgui/imgui.h"
#include "../Renderer/Renderer.h"

using namespace rkg;

namespace {
	struct {
		GLFWwindow* window;
		double time;		
		render::LayerHandle layer;
		render::UniformHandle projection_matrix;
		render::UniformHandle font_sampler;
		render::ProgramHandle program;
		render::DynamicIndexBufferHandle index_buffer;
		render::DynamicVertexBufferHandle vertex_buffer;
		float mouse_wheel;
		bool mouse_pressed[3];
	} gui;

	void Render(ImDrawData* draw_data)
	{
		using namespace render;
		constexpr uint64_t raster_state = 0 |
			RenderState::BLEND_EQUATION_ADD |
			RenderState::BLEND_ONE_MINUS_SRC_ALPHA |
			RenderState::CULL_OFF |
			RenderState::DEPTH_TEST_OFF;

		ImGuiIO& io = ImGui::GetIO();
		float fb_height = io.DisplaySize.y * io.DisplayFramebufferScale.y;
		draw_data->ScaleClipRects(io.DisplayFramebufferScale);
		//Todo: set GL viewport with screen size/viewport

		const float ortho_projection[4][4] =
		{
			{ 2.0 / io.DisplaySize.x, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 2.0f / -io.DisplaySize.y, 0.0f, 0.0f },
			{ 0.0f, 0.0f, -1.0f, 0.0f },
			{ -1.0f, 1.0f, 0.0f, 1.0f },
		};

		render::SetUniform(gui.projection_matrix, ortho_projection);

		SetState(raster_state);

		auto vert_buffer_size = 0u;
		auto index_buffer_size = 0u;


		//Copy all data to the vertex/index buffers on the GPU
		for (int n = 0; n < draw_data->CmdListsCount; n++) {
			const auto cmd_list = draw_data->CmdLists[n];
			vert_buffer_size += cmd_list->VtxBuffer.size();
			index_buffer_size += cmd_list->IdxBuffer.size();
		}
		vert_buffer_size *= sizeof(ImDrawVert);
		index_buffer_size *= sizeof(ImDrawIdx);

		auto vert_data = Alloc(vert_buffer_size);
		auto index_data = Alloc(index_buffer_size);

		uintptr_t vert_offset = 0;
		uintptr_t index_offset = 0;
		for (int n = 0; n < draw_data->CmdListsCount; n++) {
			const auto cmd_list = draw_data->CmdLists[n];
			memcpy((void*)((uintptr_t)vert_data->ptr + vert_offset), cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.size() * sizeof(ImDrawVert));
			memcpy((void*)((uintptr_t)index_data->ptr + index_offset), cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx));
			vert_offset += cmd_list->VtxBuffer.size() * sizeof(ImDrawVert);
			index_offset += cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx);
		}
		UpdateDynamicVertexBuffer(gui.vertex_buffer, vert_data);
		UpdateDynamicIndexBuffer(gui.index_buffer, index_data);

		unsigned int vtx_buffer_offset = 0;
		unsigned int idx_buffer_offset = 0;
		for (int n = 0; n < draw_data->CmdListsCount; n++) {
			const auto cmd_list = draw_data->CmdLists[n];
			
			for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++) {
				if (pcmd->UserCallback) {
					pcmd->UserCallback(cmd_list, pcmd);
				}
				else {
					SetState(raster_state);
					SetTexture(TextureHandle{ (uint32_t)pcmd->TextureId }, gui.font_sampler, 0);
					SetScissor(pcmd->ClipRect.x,
						(fb_height - pcmd->ClipRect.w),
						(pcmd->ClipRect.z - pcmd->ClipRect.x),
						(pcmd->ClipRect.w - pcmd->ClipRect.y));
					//SetVertexBuffer(gui.vertex_buffer, vtx_buffer_offset);
					//SetIndexBuffer(gui.index_buffer, idx_buffer_offset, pcmd->ElemCount);
					SetVertexBuffer(gui.vertex_buffer, vtx_buffer_offset);
					SetIndexBuffer(gui.index_buffer, idx_buffer_offset, pcmd->ElemCount);
					Submit(gui.layer, gui.program, 0, false);
				}
				idx_buffer_offset += pcmd->ElemCount;
			}
			vtx_buffer_offset += cmd_list->VtxBuffer.size();
		}
	}

	const char* GetClipboardText(void* user_data)
	{
		return glfwGetClipboardString(gui.window);
	}

	void SetClipboardText(void* user_data, const char* text)
	{
		glfwSetClipboardString(gui.window, text);
	}
}

void rkg::InitializeImgui(GLFWwindow* window)
{
	//TODO: IO setup, callbacks, etc.
	gui.window = window;
	auto& io = ImGui::GetIO();

	io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
	io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
	io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
	io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
	io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
	io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
	io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
	io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
	io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
	io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
	io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
	io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

	io.RenderDrawListsFn = Render;
	io.SetClipboardTextFn = SetClipboardText;
	io.GetClipboardTextFn = GetClipboardText;
#ifdef _WIN32
	io.ImeWindowHandle = glfwGetWin32Window(window);
#endif

	//Create fonts texture
	unsigned char* pixels;
	int width, height, bpp;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);

	auto block = render::AllocAndCopy(pixels, width*height*bpp);

	auto font_texture = render::CreateTexture2D(width, height, render::TextureFormat::RGBA8, block);
	io.Fonts->SetTexID((void*)font_texture.index);
	io.Fonts->ClearInputData();
	io.Fonts->ClearTexData();

	//Set up other rendering stuff
	const char vertex_shader[] =
		"#version 330\n"
		"uniform mat4 ProjMtx;\n"
		"in vec2 Position;\n"
		"in vec2 UV;\n"
		"in vec4 Color;\n"
		"out vec2 Frag_UV;\n"
		"out vec4 Frag_Color;\n"
		"void main()\n"
		"{\n"
		"	Frag_UV = UV;\n"
		"	Frag_Color = Color;\n"
		"	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
		"}\n";

	const char fragment_shader[] =
		"#version 330\n"
		"uniform sampler2D Texture;\n"
		"in vec2 Frag_UV;\n"
		"in vec4 Frag_Color;\n"
		"out vec4 Out_Color;\n"
		"void main()\n"
		"{\n"
		"	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
		"}\n";

	auto vert_block = render::AllocAndCopy(vertex_shader, sizeof(vertex_shader));
	auto frag_block = render::AllocAndCopy(fragment_shader, sizeof(fragment_shader));
	gui.program = render::CreateProgram(vert_block, frag_block);

	//TODO: Have to save all these handles somewhere, especially for cleanup.
	gui.font_sampler = render::CreateUniform("Texture", render::UniformType::Sampler);
	gui.projection_matrix = render::CreateUniform("ProjMtx", render::UniformType::Mat4);

	render::VertexLayout vert_layout;
	vert_layout.Add("Position", 2, render::VertexLayout::AttributeType::Float32)
		.Add("UV", 2, render::VertexLayout::AttributeType::Float32)
		.Add("Color", 4, render::VertexLayout::AttributeType::Uint8, true);

	gui.vertex_buffer = render::CreateDynamicVertexBuffer(vert_layout);
	gui.index_buffer = render::CreateDynamicIndexBuffer(render::IndexType::UShort);

	gui.layer = render::CreateLayer();
}

void rkg::ImguiNewFrame()
{
	auto& io = ImGui::GetIO();

	//Accomodate window resizing
	int w, h;
	int display_w, display_h;
	glfwGetWindowSize(gui.window, &w, &h);
	glfwGetFramebufferSize(gui.window, &display_w, &display_h);
	io.DisplaySize = ImVec2((float)display_w, (float)display_h);
	io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);

	// Setup time step
	double current_time = glfwGetTime();
	io.DeltaTime = gui.time > 0.0 ? (float)(current_time - gui.time) : (float)(1.0 / 60.0f);
	gui.time = current_time;

	//Setup inputs
	if (glfwGetWindowAttrib(gui.window, GLFW_FOCUSED)) {
		double mouse_x, mouse_y;
		glfwGetCursorPos(gui.window, &mouse_x, &mouse_y);
		io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
	}
	else {
		io.MousePos = ImVec2(-1, -1);
	}

	for (int i = 0; i < 3; i++) {
		io.MouseDown[i] = gui.mouse_pressed[i] || glfwGetMouseButton(gui.window, i) != 0;
		gui.mouse_pressed[i] = false;
	}

	io.MouseWheel = gui.mouse_wheel;
	gui.mouse_wheel = 0.0f;

	glfwSetInputMode(gui.window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

	ImGui::NewFrame();
}

void rkg::ImguiCharCallback(unsigned short c)
{
	auto& io = ImGui::GetIO();
	if (c > 0 && c < 0x10000) {
		io.AddInputCharacter(c);
	}
}

void rkg::ImguiKeyCallback(int key, int action)
{
	auto& io = ImGui::GetIO();
	if (action == GLFW_PRESS) {
		io.KeysDown[key] = true;
	}
	if (action == GLFW_RELEASE) {
		io.KeysDown[key] = false;
	}

	io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
}

void rkg::ImguiScrollCallback(float offset)
{
	gui.mouse_wheel = offset;
}

void rkg::ImguiMouseButtonCallback(int button, int action)
{
	if (action == GLFW_PRESS && button >= 0 && button < 3) {
		gui.mouse_pressed[button] = true;
	}
}