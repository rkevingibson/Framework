#include "Systems.h"

#include <vector>

#define NOMINMAX
#include "External/GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include "External/GLFW/glfw3native.h"
#include "External/imgui/imgui.h"
#include "Renderer/RenderInterface.h"
#include "Utilities/GuiBasics.h"
#include "Utilities/Utilities.h"
#include "Utilities/Input.h"

namespace rkg {
namespace ecs {

	namespace {
		std::vector<System*> systems;
		bool running;


		void GLFWErrorCallback(int error, const char* description)
		{

		}

		void ResizeCallback(GLFWwindow* win, int w, int h)
		{
			
			rkg::Input::ResizeScreen(w, h);
			rkg::render::ResizeWindow(w, h);
		}

		//TODO: Figure out how to store this user input to be used later by the systems.
		//Systems should be able to poll for this information.
		void KeyCallback(GLFWwindow* win, int key, int scancode, int action, int mods)
		{
			rkg::ImguiKeyCallback(key, action);
			if (!ImGui::GetIO().WantCaptureKeyboard) {
				//{ GLFW_RELEASE, GLFW_PRESS, GLFW_REPEAT };
				Input::SetKeyStatus(static_cast<Input::Keyname>(key), static_cast<Input::KeyAction>(action));
			}
		}

		void UnicodeCallback(GLFWwindow* win, unsigned int codepoint)
		{
			rkg::ImguiCharCallback(codepoint);
			if (!ImGui::GetIO().WantCaptureKeyboard) {

			}
		}

		void CursorCallback(GLFWwindow* win, double xpos, double ypos)
		{
			if (!ImGui::GetIO().WantCaptureMouse) {
				rkg::Input::MousePosition = Vec2{ (float)xpos, (float)ypos };
			}
		}

		void MouseButtonCallback(GLFWwindow* win, int button, int action, int mods)
		{
			rkg::ImguiMouseButtonCallback(button, action);
			if (!ImGui::GetIO().WantCaptureMouse) {
				//NOTE: This relies on GLFW not changing their mapping from buttons to numbers. 
				//Probably a bad idea to assume that won't change, but it seems reasonable. Left = 0, Right = 1, Middle = 2. Why would that change?
				bool pressed = (action == GLFW_PRESS);
				rkg::Input::MouseButton[button] = pressed;
				rkg::Input::MouseButtonPressed[button] = pressed;
				rkg::Input::MouseButtonReleased[button] = !pressed;
			}
		}

		void ScrollCallback(GLFWwindow* win, double xoffset, double yoffset)
		{
			rkg::ImguiScrollCallback(yoffset);

			if (!ImGui::GetIO().WantCaptureMouse)
			{
				rkg::Input::MouseWheelDelta = Vec2{ (float)xoffset, (float)yoffset }; //Usually we care about yoffset, maybe I should put it first?
			}
		}

		GLFWwindow* InitializeGLFW()
		{
			glfwSetErrorCallback(GLFWErrorCallback);
			if (!glfwInit())
			{
				return nullptr;
			}
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
			glfwWindowHint(GLFW_SAMPLES, 2);

			Input::ScreenSize = Vec2(600, 400);
			GLFWwindow* window = glfwCreateWindow(Input::ScreenSize.x, Input::ScreenSize.y, "Material Editor", nullptr, nullptr);
			
			if (!window)
			{
				glfwTerminate();
				return nullptr;
			}
			glfwMakeContextCurrent(window);

			glfwSetCharCallback(window, UnicodeCallback);
			glfwSetKeyCallback(window, KeyCallback);
			glfwSetMouseButtonCallback(window, MouseButtonCallback);
			glfwSetCursorPosCallback(window, CursorCallback);
			glfwSetScrollCallback(window, ScrollCallback);
			glfwSetWindowSizeCallback(window, ResizeCallback);
			return window;
		}
	}

	void AddSystem(System* system)
	{
		systems.push_back(system);
	}

	void Run()
	{
		auto window = InitializeGLFW();
		if (!window) {
			exit(EXIT_FAILURE);
		}

		rkg::render::Initialize(window); //Spawn the render thread.
		//TODO: Error callback
		rkg::InitializeImgui(window);
		for (auto system : systems) {
			system->Initialize();
		}

		running = true;
		double current_time = glfwGetTime();
		double accumulator = 0.0;
		const double fixed_timestep = 0.05;

		while (!glfwWindowShouldClose(window) && running)
		{
			//Reset inputs before processing new ones.
			for (int i = 0; i < 3; i++) {
				rkg::Input::MouseButtonPressed[i] = false;
				rkg::Input::MouseButtonReleased[i] = false;
				rkg::Input::MouseWheelDelta = { 0.0f,0.0f };
			}
			Input::NewFrame();
			glfwPollEvents();
			rkg::ImguiNewFrame();
			
			
			double new_time = glfwGetTime();
			double frame_time = new_time - current_time;
			current_time = new_time;
			accumulator += frame_time;
			while (accumulator >= fixed_timestep) {
				for (auto sys : systems) {
					sys->FixedUpdate();
				}
				accumulator -= fixed_timestep;
			}

			for (auto sys : systems) {
				sys->Update(frame_time);
			}

			//Do render here... it should probably be a hardcoded system.
			ImGui::Render();
			rkg::render::EndFrame();
		}


		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void Quit()
	{
		running = false;
	}

}//namespace ecs
}//namespace rkg