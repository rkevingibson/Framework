#pragma once
/*
	Just some helper functions to set up ImGui with glfw and the renderer.
	Doesn't override the glfw callbacks, since those are application specific.
	Instead, call the appropriate functions in the application's callbacks.

*/

struct GLFWwindow;

namespace rkg {
	void InitializeImgui(GLFWwindow* window);

	void ImguiNewFrame();

	void ImguiCharCallback(unsigned short c);
	void ImguiKeyCallback(int key, int action);
	void ImguiScrollCallback(float offset);
	void ImguiMouseButtonCallback(int button, int action);


}