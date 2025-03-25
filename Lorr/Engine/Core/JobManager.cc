#include "Engine/Core/JobManager.hh"

namespace lr {
auto Barrier::create() -> Arc<Barrier> {
    return Arc<Barrier>::create();
}

auto Barrier::wait(this Barrier &self) -> void {
    auto v = self.counter.load();
    while (v != 0) {
        self.counter.wait(v);
        v = self.counter.load();
    }
}

auto Barrier::acquire(this Barrier &self, u32 count) -> Arc<Barrier> {
    self.counter += count;
    self.acquired += count;

    return &self;
}

auto Barrier::add(this Barrier &self, Arc<Job> job) -> Arc<Barrier> {
    self.pending.push_back(std::move(job));

    return &self;
}

auto Job::create_explicit(JobFn task) -> Arc<Job> {
    ZoneScoped;

    auto job = Arc<Job>::create();
    job->task = std::move(task);

    return job;
}

auto Job::signal(this Job &self, Arc<Barrier> barrier) -> Arc<Job> {
    ZoneScoped;

    if (barrier->acquired > 0) {
        barrier->acquired--;
    } else {
        barrier->acquired++;
    }

    self.barriers.emplace_back(std::move(barrier));
    return &self;
}

JobManager::JobManager(u32 threads) {
    ZoneScoped;

    for (u32 i = 0; i < threads; i++) {
        this->workers.emplace_back([this, i]() { worker(i); });
    }
}

JobManager::~JobManager() {
    ZoneScoped;

    this->shutdown();
}

auto JobManager::shutdown(this JobManager &self) -> void {
    ZoneScoped;

    std::scoped_lock _(self.mutex);
    self.running = false;
    self.condition_var.notify_all();
}

auto JobManager::worker(this JobManager &self, u32 id) -> void {
    ZoneScoped;

    this_thread_worker.id = id;
    LS_DEFER() {
        this_thread_worker.id = ~0_u32;
    };

    while (true) {
        std::unique_lock lock(self.mutex);
        while (self.jobs.empty()) {
            if (!self.running) {
                return;
            }

            self.condition_var.wait(lock);
        }

        auto job = self.jobs.front();
        self.jobs.pop_front();
        lock.unlock();

        job->task();
        self.job_count.fetch_sub(1);

        for (auto &barrier : job->barriers) {
            if (--barrier->counter == 0) {
                for (auto &task : barrier->pending) {
                    self.submit(task, true);
                }

                barrier->counter.notify_all();
            }
        }
    }
}

auto JobManager::submit(this JobManager &self, Arc<Job> job, bool prioritize) -> void {
    ZoneScoped;

    std::unique_lock _(self.mutex);
    if (prioritize) {
        self.jobs.push_front(std::move(job));
    } else {
        self.jobs.push_back(std::move(job));
    }

    self.job_count.fetch_add(1);
    self.condition_var.notify_all();
}

auto JobManager::wait(this JobManager &self) -> void {
    ZoneScoped;

    while (self.job_count.load(std::memory_order_relaxed) != 0)
        ;
}

} // namespace lr
