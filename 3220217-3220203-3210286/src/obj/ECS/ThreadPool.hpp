#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP
#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable wait_condition;
    
    std::atomic<size_t> active_tasks{0};
    bool stop = false;

public:
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency() - 1) {
        if (num_threads == 0) num_threads = 1; 

        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { 
                            return this->stop || !this->tasks.empty(); 
                        });

                        if (this->stop && this->tasks.empty())
                            return;

                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task(); 

                    active_tasks--;
                    
                    if (active_tasks == 0) {
                        wait_condition.notify_all();
                    }
                }
            });
        }
    }

    size_t getThreadCount() const { 
        return workers.empty() ? 1 : workers.size(); 
    }
    
    template<class F>
    void enqueue(F&& f) {
        active_tasks++;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    void wait_for_all() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        wait_condition.wait(lock, [this] { return active_tasks == 0; });
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers) {
            worker.join();
        }
    }
};

#endif