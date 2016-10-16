#pragma once
namespace rkg {
namespace ecs {
	class System
	{
	public:
		virtual void Initialize() = 0;
		virtual void FixedUpdate() = 0;
		virtual void Update() = 0;
		virtual void LateUpdate() = 0;



	};	

	void AddSystem(System* );

	void FixedUpdate();
	void Update();
	void LateUpdate();

}//namespace ecs
}//namespace rkg