#pragma once
/*
	Multi-threaded job queue code. 
	Makes it easier to make multi-threaded code.
	Doesn't make it any safer though.
*/

#include <atomic>

namespace rkg {
namespace ecs {

	struct alignas(64) Job {
		using JobFunction = void(*)(Job*);

		JobFunction	fcn;
		Job*	parent{ nullptr };
		std::atomic<int32_t> unfinished_jobs{ 0 };
		std::atomic<int32_t> num_continuations{ 0 };
		Job* continuations[8];
		union { //Can store a bit of extra data, or a pointer to whatever data you need.
			void* user_ptr{ nullptr };
			char user_data[16];
		};
	};
	static_assert(sizeof(Job) <= 64, "Job got too big, something went wrong.");

	constexpr size_t MAX_NUM_JOBS = 2048; //Must be a power of two.
	constexpr size_t MAX_WORKER_THREADS = 16;

	Job* CreateJob(Job::JobFunction function);
	Job* CreateJobAsChild(Job* parent, Job::JobFunction function);
	
	void AddContinuation(Job* job, Job* continuation);
	
	void InitializeQueue(const unsigned int num_worker_threads);
	void Run(Job* job); //Add job to the job queue.
	void Wait(Job*);
	void ShutdownQueue(); //Does not flush queue.


}//namespace ecs
}//namespace rkg;