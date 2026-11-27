#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace stello::search {

/// Dynamic YBWC split node entry registered with the thread pool for zero-allocation work-stealing.
struct SplitEntry {
    SplitEntry* parent{nullptr};
    int32_t priority{0};
    size_t max_slaves{64};
    std::atomic<size_t> unclaimed{0};
    std::atomic<size_t> joining{0};
    std::atomic<size_t> active_helpers{0};
    std::atomic<bool> open{false};
    std::atomic<bool> ever_helped{false};
    std::atomic<bool> master_waiting{false};
    std::atomic<bool> wake_pending{false};
    std::mutex wait_mutex;
    std::condition_variable wait_cv;
    void (*execute)(SplitEntry* self, void* state_ptr, void* host_ctx) noexcept{nullptr};

    bool is_descendant_of(const SplitEntry* ancestor) const noexcept {
        for (const SplitEntry* cur = this; cur != nullptr; cur = cur->parent) {
            if (cur == ancestor) {
                return true;
            }
        }
        return false;
    }

    bool has_waiting_ancestor() const noexcept {
        for (const SplitEntry* cur = this; cur != nullptr; cur = cur->parent) {
            if (cur->master_waiting.load(std::memory_order_relaxed)) {
                return true;
            }
        }
        return false;
    }
};

/// Starts a thread that runs `body` (`index` is the worker's number from 0).
/// Returns true if the thread started, false if it could not start.
using Spawner = bool (*)(size_t index, std::function<void()> body);

/// Spawns a detached `std::thread`; the pool waits for all worker bodies to return in `~ThreadPool()`.
inline bool std_spawner(size_t /*index*/, std::function<void()> body) {
#if defined(__wasm__) && !defined(__wasm_atomics__)
    (void)body;
    return false;
#else
    try {
        std::thread([b = std::move(body)]() mutable { b(); }).detach();
        return true;
    } catch (...) {
        return false;
    }
#endif
}

namespace detail {

template <typename S>
class UniqueTask {
public:
    UniqueTask() noexcept = default;

    template <typename F>
    explicit UniqueTask(F&& fn)
        : impl_(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(fn))) {}

    UniqueTask(UniqueTask&&) noexcept = default;
    UniqueTask& operator=(UniqueTask&&) noexcept = default;
    UniqueTask(const UniqueTask&) = delete;
    UniqueTask& operator=(const UniqueTask&) = delete;

    void operator()(S& state) { impl_->invoke(state); }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual void invoke(S& state) = 0;
    };

    template <typename F>
    struct Model final : Concept {
        F fn;
        explicit Model(F&& f) : fn(std::move(f)) {}
        explicit Model(const F& f) : fn(f) {}
        void invoke(S& state) override { fn(state); }
    };

    std::unique_ptr<Concept> impl_;
};

struct ScopeTasks {
    std::mutex running_mutex;
    std::condition_variable finished;
    size_t running{0};

    std::mutex exception_mutex;
    std::exception_ptr exception;

    void finish() noexcept {
        std::lock_guard<std::mutex> lk(running_mutex);
        --running;
        if (running == 0) {
            finished.notify_all();
        }
    }

    void wait() noexcept {
        std::unique_lock<std::mutex> lk(running_mutex);
        finished.wait(lk, [this]() noexcept { return running == 0; });
    }

    template <typename Poll>
    void wait_polling(Poll&& poll) {
        std::unique_lock<std::mutex> lk(running_mutex);
        while (running > 0) {
            if (finished.wait_for(lk, std::chrono::milliseconds(1)) == std::cv_status::timeout && running > 0) {
                struct Relock {
                    std::unique_lock<std::mutex>& lock;
                    explicit Relock(std::unique_lock<std::mutex>& l) : lock(l) { lock.unlock(); }
                    ~Relock() { lock.lock(); }
                    Relock(const Relock&) = delete;
                    Relock& operator=(const Relock&) = delete;
                } relock(lk);
                poll();
            }
        }
    }
};

template <typename S>
struct PoolShared {
    struct StateSlot {
        std::mutex mutex;
        S state;

        explicit StateSlot(S&& s) : state(std::move(s)) {}
    };

    struct Job {
        UniqueTask<S> task;
        std::shared_ptr<ScopeTasks> scope;
    };

    std::mutex queue_mutex;
    std::deque<Job> jobs;
    std::vector<SplitEntry*> open_splits;
    bool running{true};
    size_t alive{0};

    std::condition_variable work;
    std::condition_variable changed;
    std::atomic<size_t> idle{0};
    std::atomic<size_t> waiting_masters{0};
    std::vector<std::unique_ptr<StateSlot>> states;

    SplitEntry* pick_best_split_locked(const SplitEntry* ancestor) noexcept {
        SplitEntry* best = nullptr;
        size_t best_active = 0;
        for (SplitEntry* e : open_splits) {
            const size_t active = e->active_helpers.load(std::memory_order_relaxed);
            if (e->open.load(std::memory_order_acquire)
                && active < e->max_slaves
                && e->unclaimed.load(std::memory_order_acquire) > e->joining.load(std::memory_order_acquire)
                && (ancestor == nullptr || e->is_descendant_of(ancestor)))
            {
                if (best == nullptr || e->priority > best->priority || (e->priority == best->priority && active < best_active)) {
                    best = e;
                    best_active = active;
                }
            }
        }
        if (best != nullptr) {
            best->ever_helped.store(true, std::memory_order_relaxed);
            best->active_helpers.fetch_add(1, std::memory_order_acq_rel);
            best->joining.fetch_add(1, std::memory_order_acq_rel);
        }
        return best;
    }

    static void finish_split_helper(SplitEntry* split) noexcept {
        std::lock_guard<std::mutex> wlk(split->wait_mutex);
        if (split->active_helpers.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            split->wait_cv.notify_all();
        }
    }

    void run(size_t index) {
        std::unique_lock<std::mutex> lk(queue_mutex);
        // Idle, and promised to whoever pushes the next task.
        idle.fetch_add(1, std::memory_order_relaxed);
        changed.notify_all();

        for (;;) {
            std::optional<Job> job;
            SplitEntry* split = nullptr;
            for (;;) {
                if (!jobs.empty()) {
                    job.emplace(std::move(jobs.front()));
                    jobs.pop_front();
                    break;
                }

                split = pick_best_split_locked(nullptr);
                if (split != nullptr) {
                    idle.fetch_sub(1, std::memory_order_relaxed);
                    break;
                }

                if (!running) {
                    idle.fetch_sub(1, std::memory_order_relaxed);
                    alive -= 1;
                    changed.notify_all();
                    return;
                }

                work.wait(lk);
            }

            lk.unlock();
            if (split != nullptr) {
                {
                    std::lock_guard<std::mutex> state_lk(states[index]->mutex);
                    split->execute(split, &states[index]->state, nullptr);
                }
                idle.fetch_add(1, std::memory_order_acq_rel);
                finish_split_helper(split);
                lk.lock();
                continue;
            }

            {
                std::lock_guard<std::mutex> state_lk(states[index]->mutex);
                job->task(states[index]->state);
            }
            // Release job before publishing idle/completion, and never nest queue_mutex with running_mutex.
            auto scope = std::move(job->scope);
            job.reset();

            // Idle again before the scope learns that the task finished, so a follow-up spawn finds this worker.
            idle.fetch_add(1, std::memory_order_acq_rel);
            scope->finish();

            lk.lock();
        }
    }
};

} // namespace detail

template <typename S>
class Scope {
public:
    Scope(detail::PoolShared<S>* shared, std::shared_ptr<detail::ScopeTasks> tasks) noexcept
        : shared_(shared), tasks_(std::move(tasks)) {}

    /// Runs `fn` on an idle worker and returns true, or returns false immediately if all workers are busy.
    template <typename F>
    bool try_spawn(F&& fn) const {
        // Cheap refusal first: YBWC checks at splits while all threads are busy.
        if (shared_->idle.load(std::memory_order_relaxed) == 0) {
            return false;
        }

        auto tasks = tasks_;
        detail::UniqueTask<S> wrapped(
            [fn = std::forward<F>(fn), tasks](S& state) mutable {
                try {
                    fn(state);
                } catch (...) {
                    std::lock_guard<std::mutex> elk(tasks->exception_mutex);
                    if (!tasks->exception) {
                        tasks->exception = std::current_exception();
                    }
                }
            });

        {
            std::lock_guard<std::mutex> rlk(tasks_->running_mutex);
            tasks_->running += 1;
        }
        struct RollbackRunning {
            detail::ScopeTasks* t;
            bool active{true};
            ~RollbackRunning() noexcept {
                if (active) {
                    t->finish();
                }
            }
        } rollback{tasks_.get()};

        std::lock_guard<std::mutex> lk(shared_->queue_mutex);
        if (shared_->idle.load(std::memory_order_relaxed) == 0 || !shared_->running) {
            return false;
        }

        shared_->jobs.push_back(typename detail::PoolShared<S>::Job{std::move(wrapped), tasks_});
        rollback.active = false;
        shared_->idle.fetch_sub(1, std::memory_order_relaxed);
        shared_->work.notify_one();
        return true;
    }

private:
    detail::PoolShared<S>* shared_;
    std::shared_ptr<detail::ScopeTasks> tasks_;
};

/// A fixed pool of worker threads, each owning a `S` state passed to tasks running on that worker.
template <typename S>
class ThreadPool {
public:
    template <typename MakeState>
    ThreadPool(size_t workers, Spawner spawner, MakeState&& make_state)
        : shared_(std::make_shared<detail::PoolShared<S>>()) {
        shared_->open_splits.reserve(64);
        shared_->states.reserve(workers);
        for (size_t i = 0; i < workers; ++i) {
            shared_->states.push_back(std::make_unique<typename detail::PoolShared<S>::StateSlot>(make_state()));
        }

        for (size_t index = 0; index < workers; ++index) {
            {
                std::lock_guard<std::mutex> lk(shared_->queue_mutex);
                shared_->alive += 1;
            }
            auto worker = shared_;
            if (!spawner(index, [worker = std::move(worker), index]() { worker->run(index); })) {
                std::lock_guard<std::mutex> lk(shared_->queue_mutex);
                shared_->alive -= 1;
                break;
            }
        }

        // Wait until started workers are idle before the first search runs.
        std::unique_lock<std::mutex> lk(shared_->queue_mutex);
        shared_->changed.wait(lk, [this]() noexcept {
            return shared_->idle.load(std::memory_order_relaxed) >= shared_->alive;
        });
    }

    ~ThreadPool() {
        std::unique_lock<std::mutex> lk(shared_->queue_mutex);
        shared_->running = false;
        shared_->work.notify_all();
        shared_->changed.wait(lk, [this]() noexcept { return shared_->alive == 0; });
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    size_t workers() const {
        std::lock_guard<std::mutex> lk(shared_->queue_mutex);
        return shared_->alive;
    }

    size_t idle() const noexcept {
        return shared_->idle.load(std::memory_order_relaxed);
    }

    bool has_free_helper(const SplitEntry* current_entry = nullptr) const noexcept {
        if (shared_->idle.load(std::memory_order_relaxed) > 0) {
            return true;
        }
        if (current_entry != nullptr && shared_->waiting_masters.load(std::memory_order_relaxed) > 0) {
            return current_entry->has_waiting_ancestor();
        }
        return false;
    }

    void publish_split(SplitEntry& entry, size_t max_wake) const {
        size_t wakes = 0;
        {
            std::lock_guard<std::mutex> lk(shared_->queue_mutex);
            entry.open.store(true, std::memory_order_release);
            shared_->open_splits.push_back(&entry);
            const size_t avail = shared_->idle.load(std::memory_order_relaxed);
            wakes = (avail < max_wake) ? avail : max_wake;
        }
        for (size_t i = 0; i < wakes; ++i) {
            shared_->work.notify_one();
        }
        if (wakes < max_wake && shared_->waiting_masters.load(std::memory_order_seq_cst) > 0) {
            size_t rem = max_wake - wakes;
            for (SplitEntry* p = entry.parent; p != nullptr && rem > 0; p = p->parent) {
                if (p->master_waiting.load(std::memory_order_seq_cst)) {
                    {
                        std::lock_guard<std::mutex> wlk(p->wait_mutex);
                        p->wake_pending.store(true, std::memory_order_relaxed);
                    }
                    p->wait_cv.notify_all();
                    --rem;
                }
            }
        }
    }

    void unpublish_split(SplitEntry& entry) const noexcept {
        std::lock_guard<std::mutex> lk(shared_->queue_mutex);
        entry.open.store(false, std::memory_order_release);
        auto& vec = shared_->open_splits;
        for (size_t i = 0; i < vec.size(); ++i) {
            if (vec[i] == &entry) {
                vec[i] = vec.back();
                vec.pop_back();
                break;
            }
        }
    }

    template <typename Poll>
    void wait_split(SplitEntry& entry, S& master_state, void* host_ctx, Poll&& poll) const {
        unpublish_split(entry);
        if (entry.active_helpers.load(std::memory_order_acquire) == 0) {
            if (entry.ever_helped.load(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> drain(entry.wait_mutex);
            }
            return;
        }

        while (entry.active_helpers.load(std::memory_order_acquire) > 0) {
            SplitEntry* desc = nullptr;
            {
                std::lock_guard<std::mutex> lk(shared_->queue_mutex);
                desc = shared_->pick_best_split_locked(&entry);
            }
            if (desc != nullptr) {
                desc->execute(desc, &master_state, host_ctx);
                detail::PoolShared<S>::finish_split_helper(desc);
                continue;
            }

            entry.wake_pending.store(false, std::memory_order_seq_cst);
            entry.master_waiting.store(true, std::memory_order_seq_cst);
            shared_->waiting_masters.fetch_add(1, std::memory_order_seq_cst);
            {
                std::lock_guard<std::mutex> lk(shared_->queue_mutex);
                desc = shared_->pick_best_split_locked(&entry);
            }
            if (desc != nullptr) {
                entry.master_waiting.store(false, std::memory_order_relaxed);
                shared_->waiting_masters.fetch_sub(1, std::memory_order_relaxed);
                desc->execute(desc, &master_state, host_ctx);
                detail::PoolShared<S>::finish_split_helper(desc);
                continue;
            }

            {
                std::unique_lock<std::mutex> wlk(entry.wait_mutex);
                if (entry.active_helpers.load(std::memory_order_acquire) > 0
                    && !entry.wake_pending.load(std::memory_order_acquire))
                {
                    entry.wait_cv.wait_for(wlk, std::chrono::milliseconds(1));
                }
            }
            entry.master_waiting.store(false, std::memory_order_relaxed);
            shared_->waiting_masters.fetch_sub(1, std::memory_order_relaxed);
            poll();
        }
        if (entry.ever_helped.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> drain(entry.wait_mutex);
        }
    }

    template <typename F>
    void for_each_state(F&& fn) const {
        for (const auto& slot : shared_->states) {
            std::lock_guard<std::mutex> lk(slot->mutex);
            fn(slot->state);
        }
    }

    template <typename F>
    auto scope(F&& body) const -> decltype(body(std::declval<const Scope<S>&>())) {
        return run_scope<F, std::nullptr_t>(std::forward<F>(body), nullptr);
    }

    template <typename F, typename Poll>
    auto scope_polling(F&& body, Poll&& poll) const -> decltype(body(std::declval<const Scope<S>&>())) {
        return run_scope(std::forward<F>(body), &poll);
    }

private:
    template <typename F, typename Poll>
    auto run_scope(F&& body, Poll* poll) const -> decltype(body(std::declval<const Scope<S>&>())) {
        auto tasks = std::make_shared<detail::ScopeTasks>();
        Scope<S> scope(shared_.get(), tasks);

        struct WaitOnDrop {
            detail::ScopeTasks* tasks;
            explicit WaitOnDrop(detail::ScopeTasks* t) noexcept : tasks(t) {}
            ~WaitOnDrop() noexcept { tasks->wait(); }
            WaitOnDrop(const WaitOnDrop&) = delete;
            WaitOnDrop& operator=(const WaitOnDrop&) = delete;
        };

        using ResultT = decltype(body(scope));
        if constexpr (std::is_void_v<ResultT>) {
            {
                WaitOnDrop wait_on_drop(tasks.get());
                body(scope);
                if constexpr (!std::is_same_v<Poll, std::nullptr_t>) {
                    if (poll != nullptr) {
                        tasks->wait_polling(*poll);
                    }
                }
            }
            if (tasks->exception) {
                std::rethrow_exception(tasks->exception);
            }
        } else {
            std::optional<ResultT> result;
            {
                WaitOnDrop wait_on_drop(tasks.get());
                result.emplace(body(scope));
                if constexpr (!std::is_same_v<Poll, std::nullptr_t>) {
                    if (poll != nullptr) {
                        tasks->wait_polling(*poll);
                    }
                }
            }
            if (tasks->exception) {
                std::rethrow_exception(tasks->exception);
            }
            return std::move(*result);
        }
    }

    std::shared_ptr<detail::PoolShared<S>> shared_;
};

} // namespace stello::search
