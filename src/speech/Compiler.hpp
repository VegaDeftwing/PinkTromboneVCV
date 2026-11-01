#pragma once
#include "English.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <memory>

namespace pinkSpeech {
// The worker owns all allocations and reclamation. The audio thread only moves
// raw immutable plan pointers through the mailbox and intrusive retired list.
class Compiler {
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::string text, error;
    bool stopping = false;
    uint64_t ready = 0;
    size_t sounds = 0;
    std::atomic<uint64_t> requested{0};
    std::atomic<bool> loading{false};
    std::atomic<Plan*> pending{nullptr};
    std::atomic<Plan*> retired{nullptr};
    std::thread worker;
    void reclaim() {
        Plan* list = retired.exchange(nullptr, std::memory_order_acquire);
        while (list) { Plan* next = list->retiredNext; delete list; list = next; }
    }
    void run() {
        uint64_t handled = 0;
        for (;;) {
            reclaim();
            std::string input;
            uint64_t revision;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait_for(lock, std::chrono::milliseconds(50), [&] { return stopping || requested.load() != handled; });
                if (stopping) break;
                revision = requested.load();
                if (revision == handled) continue;
                // Debounce typing, while allowing stop/join to wake immediately.
                if (changed.wait_for(lock, std::chrono::milliseconds(60), [&] { return stopping || requested.load() != revision; })) continue;
                input = text; handled = revision;
            }
            try {
                std::unique_ptr<Plan> plan(new Plan(compileEnglish(input)));
                plan->revision = revision;
                std::lock_guard<std::mutex> lock(mutex);
                if (stopping) break;
                if (revision != requested.load()) continue;
                sounds = plan->segments.size();
                ready = revision;
                delete pending.exchange(plan.release(), std::memory_order_acq_rel);
                error.clear(); loading.store(false, std::memory_order_release);
            } catch (const std::exception& exception) {
                std::lock_guard<std::mutex> lock(mutex);
                if (revision == requested.load()) { error = exception.what(); loading.store(false, std::memory_order_release); }
            }
        }
        reclaim();
    }
public:
    struct Info { std::string text, error; uint64_t requested = 0, ready = 0; size_t sounds = 0; bool loading = false; };
    Compiler() {
        request("Hello. This is Pink Trombone.");
        worker = std::thread([this] { run(); });
    }
    ~Compiler() {
        { std::lock_guard<std::mutex> lock(mutex); stopping = true; }
        changed.notify_one();
        worker.join();
        delete pending.exchange(nullptr);
        reclaim();
    }
    Compiler(const Compiler&) = delete;
    Compiler& operator=(const Compiler&) = delete;
    void request(const std::string& input) {
        { std::lock_guard<std::mutex> lock(mutex);
          if (text == input && requested.load()) return;
          text = input; error.clear(); requested.fetch_add(1, std::memory_order_release); loading.store(true, std::memory_order_release); }
        changed.notify_one();
    }
    Info info() const {
        std::lock_guard<std::mutex> lock(mutex);
        Info result;
        result.text = text; result.error = error; result.requested = requested.load();
        result.ready = ready; result.sounds = sounds; result.loading = loading.load();
        return result;
    }
    uint64_t revision() const { return requested.load(std::memory_order_acquire); }
    Plan* take() { return pending.exchange(nullptr, std::memory_order_acq_rel); }
    void retire(Plan* plan) {
        if (!plan) return;
        Plan* head = retired.load(std::memory_order_relaxed);
        do { plan->retiredNext = head; } while (!retired.compare_exchange_weak(head, plan, std::memory_order_release, std::memory_order_relaxed));
    }
};
}
