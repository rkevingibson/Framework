#include "Systems.h"

#include <vector>

#define NOMINMAX
#include "External/GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include "External/GLFW/glfw3native.h"
#include "External/imgui/imgui.h"
#include "Renderer/Renderer.h"
#include "Utilities/GuiBasics.h"
#include "Utilities/Utilities.h"

namespace rkg {
namespace ecs {

	namespace {
		std::vector<System*> systems;
		bool running;


		static void GLFWErrorCallback(int error, const char* description)
		{

		}

		static void ResizeCallback(GLFWwindow* win, int w, int h)
		{
			rkg::render::Resize(w, h);
		}

		//TODO: Figure out how to store this user input to be used later by the systems.
		//Systems should be able to poll for this information.
		static void KeyCallback(GLFWwindow* win, int key, int scancode, int action, int mods)
		{
			rkg::ImguiKeyCallback(key, action);
			if (!ImGui::GetIO().WantCaptureKeyboard) {

			}
		}

		static void UnicodeCallback(GLFWwindow* win, unsigned int codepoint)
		{
			rkg::ImguiCharCallback(codepoint);
			if (!ImGui::GetIO().WantCaptureKeyboard) {

			}
		}

		static void CursorCallback(GLFWwindow* win, double xpos, double ypos)
		{
			if (!ImGui::GetIO().WantCaptureMouse) {
			
			}
		}

		static void MouseButtonCallback(GLFWwindow* win, int button, int action, int mods)
		{
			rkg::ImguiMouseButtonCallback(button, action);
			if (!ImGui::GetIO().WantCaptureMouse) {
				if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {

					double x, y;
					glfwGetCursorPos(win, &x, &y);
				}
				else {
				}
			}
		}

		static void ScrollCallback(GLFWwindow* win, double xoffset, double yoffset)
		{
			rkg::ImguiScrollCallback(yoffset);

			if (!ImGui::GetIO().WantCaptureMouse)
			{
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

			GLFWwindow* window = glfwCreateWindow(600, 400, "Material Editor", nullptr, nullptr);

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

		rkg::render::Initialize(window);
		//TODO: Error callback
		rkg::InitializeImgui(window);

		for (auto system : systems) {
			system->Initialize();
		}

		running = true;
		double current_time = glfwGetTime();
		double accumulator = 0.0;
		const double fixed_timestep = 0.01;

		while (!glfwWindowShouldClose(window) && running)
		{
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
			rkg::render::EndFrame();
		}
	}

	void Quit()
	{
		running = false;
	}

}//namespace ecs
}//namespace rkg