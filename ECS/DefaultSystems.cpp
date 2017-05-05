#include <ECS/DefaultSystems.h>
#include <Utilities/Input.h>
#include <External/imgui/imgui.h>
#include <Renderer/ArcballCamera.h>
#include <Renderer/RenderInterface.h>


using namespace rkg;

void DeveloperConsole::Initialize()
{

}

void DeveloperConsole::Update(double delta_time)
{
	if (Input::GetKeyDown(Input::Keyname::GRAVE_ACCENT)) {
		show_console = !show_console;
	}

	if (show_console) {

		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiSetCond_Always);
		ImGui::SetNextWindowSize(ImVec2(Input::ScreenSize.x, Input::ScreenSize.y*0.2), ImGuiSetCond_Always);
		ImGui::Begin("Developer Console", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		char buffer[256]{ 0 };

		//
		// Draw the history - say, last 5 commands.
		//
		ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
		for (int i = std::min<int>(num_history_displayed, history.size()); i > 0; i--) {
			auto& cmd = *(history.rbegin() + i - 1);
			for (auto token : cmd) {
				ImGui::TextUnformatted(token.c_str());
				ImGui::SameLine();
			}
			ImGui::NewLine();
		}
		ImGui::SetScrollHere();
		ImGui::EndChild();
		ImGui::Separator();
		if (ImGui::InputText("Input", buffer, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
			char* token = strtok(buffer, " ");
			Command cmd;
			while (token != nullptr) {
				cmd.push_back(token);
				token = std::strtok(NULL, " ");
			}

			if (history.size() == max_history_size) {
				history.erase(history.begin());
			}

			history.push_back(cmd);

			if (cmd.size() > 0) {
				auto fn = command_list.find(cmd[0]);
				if (fn != command_list.end()) {
					if (cmd.size() == 1) {
						fn->second(0, nullptr);
					} else {
						fn->second(cmd.size() - 1, &cmd[1]);
					}
				} else {
					printf("Command not found!");
				}
			}
			ImGui::SetKeyboardFocusHere(-1);
		}
		ImGui::End();
	}
}

void DeveloperConsole::AddCommand(const char* name, CommandFn fn)
{
	command_list[name] = fn;
}


void ArcballSystem::Initialize()
{
	arcball = std::make_unique<ArcballCamera>();
	arcball->EndArcball();
	auto resize = [](int w, int h, void* data) {
		ArcballCamera* arcball = reinterpret_cast<ArcballCamera*>(data);
		arcball->screen_size = Vec2(w, h);
	};
	Input::RegisterResizeCallback(resize, arcball.get());
	arcball->screen_size = Input::ScreenSize;
}


void ArcballSystem::Update(double delta_time)
{
	if (Input::MouseButtonPressed[0]) {
		arcball->StartArcball(Input::MousePosition);
	}
	else if (Input::MouseButtonReleased[0]) {
		arcball->EndArcball();
	}

	arcball->UpdateArcball(Input::MousePosition);
	arcball->distance -= zoom_speed*Input::MouseWheelDelta.y;
	render::SetViewTransform(arcball->GetViewMatrix());
}

void ArcballSystem::SetTarget(const Vec3& target, float distance) 
{
	arcball->target = target;
	arcball->distance = distance;
}