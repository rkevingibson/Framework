#include "Jobs.h"
#include <array>
#include <thread>

#include "Utilities/Utilities.h"

namespace rkg {
namespace job {
	namespace {
		std::array<Job, MAX_NUM_JOBS> global_job_list;
		std::atomic<uint32_t> job_allocation_index;

		Job* Allocate()
		{
			uint32_t index = job_allocation_index.fetch_add(1);
			//TODO: Ensure that I'm not overwriting jobs. Some maximum enforcement would be a good idea.

			return &global_job_list[index & (MAX_NUM_JOBS - 1)];
		}

		std::atomic_bool queues_running{ true };
		std::atomic_int active_workers{ 0 };

		class alignas(64) JobQueue
		{
			/*
			Lock-free queue.
			Assumptions: Pop and Push are only 
			called by one thread. 
			Steal is called by any other thread.
			*/
		private:
			std::array<Job*, MAX_NUM_JOBS> data_;
			static constexpr auto MASK = MAX_NUM_JOBS - 1;
			std::atomic<unsigned int> bottom_;
			std::atomic<unsigned int> top_;
		public:
			Job* Pop() 
			{
				unsigned int b = bottom_ - 1;
				bottom_ = b;

				unsigned int t = top_;
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
						bottom_ = t;
						return nullptr;
					}
				}
				else
				{
					bottom_ = t;//Queue was already empty
					return nullptr;
				}
			}
			
			void Push(Job* job)
			{
				unsigned int b = bottom_;
				data_[b  & MASK] = job;
				bottom_ = b + 1;
			}

			Job* Steal()
			{
				auto t = top_.load();
				auto b = bottom_.load();
				if (t < b) 
				{
					Job* job = data_[t & MASK];

					//If someone else has stolen already, this will fail.
					if (!top_.compare_exchange_strong(t, t+1))
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

		};


		JobQueue queues[MAX_WORKER_THREADS];

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

		Job* GetJob(const int thread_index)
		{
			auto job = queues[thread_index].Pop();
			if (job == nullptr)
			{
				//pick another thread at random, 
				int num_threads = active_workers;
				//Need to get some fast random number generator.
				auto random_index = GetRandomThreadIndex(num_threads);
				if (random_index == thread_index) {
					return nullptr;//Don't steal from ourselves.
				}
				job = queues[random_index].Steal();
			}

			return job;
		}

		void Finish(Job* job)
		{
			const int32_t unfinished_jobs = job->unfinished_jobs.fetch_add(-1);
			if (unfinished_jobs == 0) 
			{ //All children are done too.
				if (job->parent) {//Signal to the parent that this child is done.
					Finish(job->parent);
				}

				int32_t num_dependencies = job->num_dependencies;
				for (int i = 0; i < num_dependencies; i++) {
					Run(job->dependency[i]);
				}
			}
		}

		thread_local int current_thread_index{ 0 };

		void WorkerFunction(const int thread_index)
		{
			active_workers++;
			current_thread_index = thread_index;
			while (queues_running) 
			{
				//Get a job
				auto job = GetJob(current_thread_index);
				if (job == nullptr) {
					std::this_thread::yield();
				}
				else {
					job->fcn(job);
				}
				Finish(job);
			}
			active_workers--;
		}

	}//Anonymous namespace



	Job * CreateJob(Job::JobFunction function)
	{
		auto job = Allocate();
		job->fcn = function;
		job->unfinished_jobs = 1;
		return job;
	}

	Job * CreateJobAsChild(Job* parent, Job::JobFunction function)
	{
		auto job = Allocate();
		job->fcn = function;
		job->parent = parent;
		parent->unfinished_jobs++;
		job->unfinished_jobs = 1;
		return job;
	}

	void AddDependency(Job* job, Job* dependency)
	{
		//Worried about a race condition here - what if the job is Finished while this is added?
		//Dependencies should only be added from within the Job Function, then its safe.
		const int32_t i = job->num_dependencies.fetch_add(1);
		job->dependency[i] = dependency;
	}

	void InitializeQueue(const unsigned int num_worker_threads)
	{
		ASSERT(num_worker_threads < MAX_WORKER_THREADS);
		for (int i = 0; i < num_worker_threads; i++)
		{
			std::thread t(WorkerFunction, i+1);
			t.detach();//Release it into the wild!
		}
	}

	void Run(Job* job)
	{
		queues[current_thread_index].Push(job);
	}

	void Wait(Job* job)
	{
		while (job->unfinished_jobs != 0)
		{
			Job* j = GetJob(current_thread_index);
			if (j) {
				j->fcn(j);
			}
		}
	}

	void ShutdownQueue()
	{
		queues_running = false;
		while (active_workers != 0) {
			std::this_thread::yield();
		}
	}

}//namespace job
}//namespace rkg