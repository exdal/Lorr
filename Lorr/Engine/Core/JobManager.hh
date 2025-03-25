#pragma once

#include "Engine/Core/Arc.hh"

#include <condition_variable>
#include <shared_mutex>
#include <thread>

namespace lr {
using JobFn = std::function<void()>;

struct Job;
struct Barrier : ManagedObj {
    u32 acquired = 0;
    std::atomic<u32> counter = 0;
    std::vector<Arc<Job>> pending = {};

    static auto create() -> Arc<Barrier>;
    auto wait(this Barrier &self) -> void;
    auto acquire(this Barrier &self, u32 count = 1) -> Arc<Barrier>;
    auto add(this Barrier &self, Arc<Job> job) -> Arc<Barrier>;
};

struct JobManager;
struct Job : ManagedObj {
    std::vector<Arc<Barrier>> barriers = {};
    JobFn task = {};

    static auto create_explicit(JobFn task) -> Arc<Job>;
    template<typename Fn>
    static auto create(Fn task) -> Arc<Job> {
        return create_explicit(std::forward<Fn>(task));
    }

    auto signal(this Job &self, Arc<Barrier> barrier) -> Arc<Job>;
};

struct ThreadWorker {
    u32 id = ~0_u32;
};

inline thread_local ThreadWorker this_thread_worker;

struct JobManager {
private:
    std::vector<std::jthread> workers = {};
    std::deque<Arc<Job>> jobs = {};
    std::shared_mutex mutex = {};
    std::condition_variable_any condition_var = {};
    std::atomic<u64> job_count = {};
    bool running = true;

public:
    JobManager(u32 threads);
    ~JobManager();

    auto shutdown(this JobManager &self) -> void;
    auto worker(this JobManager &self, u32 id) -> void;
    auto submit(this JobManager &self, Arc<Job> job, bool prioritize = false) -> void;
    auto wait(this JobManager &self) -> void;
};

} // namespace lr
