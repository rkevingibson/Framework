#pragma once

#include <atomic>

namespace rkg {
namespace ecs
{
struct Job {
	static constexpr size_t PADDING_SIZE{ 44 };

	using JobFn = void(*)(void*, Job*);
	JobFn function;
	Job* parent{ nullptr };
	std::atomic<uint32_t> unfinished_jobs{ 0 }; //Itself, plus any children jobs.
	char padding[PADDING_SIZE];
};

void InitializeWorkerThreads(int num_workers);
void ShutdownWorkerThreads();
void SubmitJob(Job* j);


//This should never be used directly - not part of the public interface really.
Job* AllocateJob(int extra_space);

template<typename T>
Job* CreateJob(T&& fn)
{
	//Takes any function which has no arguments, and makes a job for it. 
	//First, we need to allocate the job. 

	//TODO: Lambda alignment requirements? Haven't considered it.

	//We allocate the job starting at the padding, but we may need extra space for larger lambdas (with lots of captures)
	int extra_space = std::max<int>(sizeof(T) - Job::PADDING_SIZE, 0);
	Job* job = AllocateJob(extra_space);

	ASSERT(job != nullptr && "Job failed to allocate!!");

	new(job->padding) T(std::forward<T>(fn));
	job->function = [](void* address, Job* j) {
		auto f = reinterpret_cast<T*>(address);
		f->operator()(j);
	};
	job->unfinished_jobs = 1;

	return job;
}

template<typename T>
Job* CreateChildJob(Job* parent, T&& fn)
{
	++parent->unfinished_jobs;
	Job* job = CreateJob(std::forward<T>(fn));
	job->parent = parent;

	return job;
}


//DON'T CALL THIS FUNCTION DIRECTLY! NOT PART OF THE PUBLIC INTERFACE!
template<typename T>
void ParallelForHelper(Job* j, size_t count, size_t batch_size, size_t offset, T&& fn)
{
	if (count > batch_size) {
		//Split.
		size_t left_count = count / 2;
		size_t right_count = count - left_count;

		Job* left = CreateChildJob(j, [=](Job* job) {
			ParallelForHelper(job, left_count, batch_size, offset, fn);
		});

		Job* right = CreateChildJob(j, [=](Job* job) {
			ParallelForHelper(job, right_count, batch_size, offset + left_count, fn);
		});

		SubmitJob(left);
		SubmitJob(right);
	}
	else {
		//Execute the function.
		for (int i = 0; i < count; i++) {
			fn(offset + i);
		}
	}
	return;
}

//Splits a job into a number of batches, to be executed as jobs.
template<typename T>
Job* ParallelFor(size_t count, size_t batch_size, const T& fn)
{
	//It would be best to split the job up not all at once, but in parallel, so that it can be separated over multiple queues. 
	//Right now that isn't possible - may need to change my job system.

	auto root = CreateJob([=](Job* j) {
		ParallelForHelper(j, count, batch_size, 0, fn);
	});
	return root;
}

void Wait(Job* j);

int GetThreadIndex();

void ClearJobs();
}
}