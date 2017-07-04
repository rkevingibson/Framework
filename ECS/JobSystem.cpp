#include "JobSystem.h"

#include <cstdint>
#include <new>
#include <thread>
#include <type_traits>
#include <vector>
#include <algorithm>
#include <memory>
#include <array>

#include "Utilities\Utilities.h"
#include "Utilities\Allocators.h"

namespace rkg {
	namespace ecs {

namespace {
	static constexpr auto SIZE = sizeof(Job);
	static constexpr auto CACHE_SIZE = 64;

	static_assert(SIZE - CACHE_SIZE == 0, "Need to adjust padding of Job!");

	static constexpr size_t MAX_NUM_JOBS = KILO(128);

	class alignas(64) JobQueue
	{
		/*
		Lock-free queue.
		Assumptions: Pop and Push are only
		called by one thread.
		Steal is called by any other thread.
		*/
	private:

		//TODO: Replace this with a linear allocator.
		std::array<Job*, MAX_NUM_JOBS> data_;
		static constexpr auto MASK = MAX_NUM_JOBS - 1;
		std::atomic<int> bottom_;
		std::atomic<int> top_;
	public:
		Job* Pop()
		{
			auto b = bottom_.load() - 1;
			bottom_.store(b);

			auto t = top_.load();
			if (t <= b)
			{
				auto job = data_[b & MASK];
				if (t != b) {
					return job;
				}
				//This is the last item in the queue.
				if (top_.compare_exchange_strong(t, t + 1))
				{ //Compare succeeded, we got the last item.
					bottom_ = t + 1;
					return job;
				}
				else
				{
					//Compare failed, which means a steal has incremented top_.
					//Since the compare/exchange incremented t, we just do this.
					bottom_.store(t);
					return nullptr;
				}
			}
			else
			{
				bottom_.store(t);//Queue was already empty
				return nullptr;
			}
		}

		void Push(Job* job)
		{
			auto b = bottom_.load();
			data_[b  & MASK] = job;
			bottom_.store(b + 1);
		}

		Job* Steal()
		{
			auto t = top_.load();
			auto b = bottom_.load();
			if (t < b)
			{
				Job* job = data_[t & MASK];

				//If someone else has stolen already, this will fail.
				if (!top_.compare_exchange_strong(t, t + 1))
				{
					return nullptr;
				}
				return job;
			}
			else
			{
				return nullptr;//Empty queue to steal from.
			}
		}

		//Assumes that all jobs are executed already - happens at end of a frame.
		void Clear()
		{
			//This can be called while both Pop and Steal are being called. 
			//So it needs to make sure those stay valid. 

			bottom_.store(0);
			top_.store(0);
		}

	};

	using JobAllocator = rkg::GrowingLinearAllocator<MAX_NUM_JOBS * sizeof(Job)>;
	std::unique_ptr<JobQueue[]> job_queues;
	std::unique_ptr<JobAllocator[]> job_allocators;
	int num_job_queues;
	thread_local int thread_index{ 0 };
	std::atomic_bool workers_running{ false };
	std::atomic_int clear_workers{ 0 };

	unsigned int GetRandomThreadIndex(const int num_threads)
	{
		//Borrowed from Stack overflow http://stackoverflow.com/questions/1640258/need-a-fast-random-generator-for-c
		static unsigned long x = 123456789, y = 362436069, z = 521288629;

		unsigned long t;
		x ^= x << 16;
		x ^= x >> 5;
		x ^= x << 1;

		t = x;
		x = y;
		y = z;
		z = t ^ x ^ y;

		return z % num_threads;
	}

	Job* GetJob()
	{
		//Grab a job from the queue, or steal from another thread if none available.
		Job* job = job_queues[thread_index].Pop();

		if (!job) {


			int random_index = GetRandomThreadIndex(num_job_queues); //Should probably randomize this properly.

			if (random_index == thread_index) {
				std::this_thread::yield();
				return nullptr;
			}
			else {
				job = job_queues[random_index].Steal(); //Try to steal. 
				if (!job) {
					return nullptr;
				}
				else {
					return job;
				}

			}
		}
		else {
			return job;
		}

	}

	void Finish(Job* j)
	{
		const int32_t unfinishedJobs = --(j->unfinished_jobs);
		if (unfinishedJobs == 0)
		{
			//TODO: Add to list of jobs that need to be deleted. Don't actively delete it yet.
			if (j->parent)
			{
				Finish(j->parent);
			}
			--(j->unfinished_jobs);
		}
	}

	void Execute(Job* j)
	{
		//Run the job.
		(j->function)(j->padding, j);
		Finish(j);
	}

}

void InitializeWorkerThreads(int num_workers)
{
	num_job_queues = num_workers + 1; //Add one for this thread as well.
	workers_running = true;
	job_queues = std::make_unique<JobQueue[]>(num_job_queues);
	job_allocators = std::make_unique<JobAllocator[]>(num_job_queues);
	for (int i = 0; i < num_workers; i++) {
		std::thread worker([](int index) {
			thread_index = index;
			while (workers_running) {
				auto job = GetJob();
				if (job) {
					Execute(job);
				}
				else {
					if (clear_workers == GetThreadIndex()) {
						job_queues[GetThreadIndex()].Clear();
						job_allocators[GetThreadIndex()].DeallocateAll();
						clear_workers.fetch_sub(1);
					}

					std::this_thread::yield();
				}
			}
		}, i + 1);

		worker.detach();
	}


}

void ShutdownWorkerThreads()
{
	workers_running = false;
}

void SubmitJob(Job* j)
{
	//Add to the queue.
	job_queues[thread_index].Push(j);

}

Job* AllocateJob(int extra_space)
{
	//For now, just allocate with malloc.

	auto job_block = job_allocators[thread_index].Allocate(rkg::RoundToAligned(sizeof(Job) + extra_space, 64));
	return reinterpret_cast<Job*>(job_block.ptr);
}

void Wait(Job* j)
{
	while (j->unfinished_jobs != -1) {
		//Work the queue.
		Job* next_job = GetJob();
		if (next_job) {
			Execute(next_job);
		}
	}
}

int GetThreadIndex()
{
	return thread_index;
}

void ClearJobs()
{
	clear_workers.store(num_job_queues - 1);
	job_queues[GetThreadIndex()].Clear();
	job_allocators[GetThreadIndex()].DeallocateAll();
	//printf("Clear_workers:%d", clear_workers.load());
	while (clear_workers.load() != 0) {
		std::this_thread::yield();
	}

}

template<typename T>
Job* ParallelFor(size_t count, T&& fn)
{


	return nullptr;
}

}
}