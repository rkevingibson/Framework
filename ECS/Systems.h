#pragma once
namespace rkg
{
namespace ecs
{
class System
{
public:
	virtual void Initialize() {};
	virtual void FixedUpdate() {};
	virtual void Update(double delta_time) {};
	virtual void LateUpdate() {};
};

void AddSystem(System*);
void Run();
void Quit();

}//namespace ecs
}//namespace rkg