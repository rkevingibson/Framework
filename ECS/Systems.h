#pragma once
namespace rkg {
namespace ecs {
	class System
	{
	public:
		virtual void Initialize() = 0;
		virtual void FixedUpdate() = 0;
		virtual void Update(double delta_time) = 0;
		virtual void LateUpdate() = 0;
	};	

	void AddSystem(System* );
	void Run();
	void Quit();

}//namespace ecs
}//namespace rkg