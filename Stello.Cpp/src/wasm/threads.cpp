#include "wasm/threads.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <utility>

namespace stello::wasm {
namespace {

constexpr auto kReturnWait = std::chrono::milliseconds(200);

struct QueueState {
    std::mutex mutex;
    std::condition_variable queued;
    std::condition_variable freed;
    std::deque<std::function<void()>> bodies;
    size_t idle{0};
    size_t running{0};
};

QueueState& queue() noexcept {
    static QueueState instance;
    return instance;
}

} // namespace

bool web_spawner(size_t /*index*/, std::function<void()> body) {
    QueueState& q = queue();
    std::unique_lock<std::mutex> lk(q.mutex);
    bool waited = false;
    while (q.idle == 0) {
        if (q.running == 0 || waited) {
            return false;
        }
        waited = !q.freed.wait_for(lk, kReturnWait, [&q]() noexcept {
            return q.idle > 0 || q.running == 0;
        });
    }

    --q.idle;
    ++q.running;
    q.bodies.push_back(std::move(body));
    q.queued.notify_one();
    return true;
}

size_t idle_threads() {
    QueueState& q = queue();
    std::lock_guard<std::mutex> lk(q.mutex);
    return q.idle;
}

[[noreturn]] void pool_worker() {
    QueueState& q = queue();
    std::unique_lock<std::mutex> lk(q.mutex);
    for (;;) {
        ++q.idle;
        q.freed.notify_all();
        while (q.bodies.empty()) {
            q.queued.wait(lk);
        }
        auto body = std::move(q.bodies.front());
        q.bodies.pop_front();
        lk.unlock();
        try {
            body();
        } catch (...) {
        }
        lk.lock();
        --q.running;
    }
}

} // namespace stello::wasm
