#pragma once

#include <string>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <memory>

#include <Utilities/Geometry.h>
#include <ECS/Systems.h>

namespace rkg
{

class DeveloperConsole : public ecs::System
{
public:
	using CommandFn = void(*)(int, std::string*);

	virtual void Initialize() override;
	virtual void Update(double delta_time) override;

	void AddCommand(const char* name, CommandFn function);
private:
	using Command = std::vector<std::string>;

	const int max_history_size{ 25 };
	const int num_history_displayed{ 25 };
	std::vector<Command> history;
	bool show_console{ false };
	std::unordered_map<std::string, CommandFn> command_list;
};


class ArcballCamera;

class ArcballSystem : public ecs::System
{
public:
	virtual void Initialize() override;
	virtual void Update(double delta_time) override;

	void SetTarget(const Vec3& target, float distance);
	inline void SetZoomSpeed(float speed) {
		zoom_speed = speed;
	}
private:
	std::unique_ptr<ArcballCamera> arcball;
	float zoom_speed;
};

}