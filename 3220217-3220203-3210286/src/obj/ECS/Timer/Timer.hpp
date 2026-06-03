#ifndef TIMER_HPP
#define TIMER_HPP
#pragma once

#include <chrono>
#include <iostream>

class Timer {
  private:
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds> _start;

  public:
    Timer() {
        _start = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now());
    }

    ~Timer() {
        decltype(_start) _end = std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now());

        auto duration = _end - _start;

        std::cout << "Duration of scope: " << duration << "\n\n";
    }
};

#define __TIME_IT__ Timer timer;

#endif