#pragma once

#include "Types.h"
#include <chrono>

namespace WorldEditor {

class Timer {
public:
    Timer();

    void reset();
    f64 elapsed() const; // Returns elapsed time in seconds
    f64 elapsedMillis() const; // Returns elapsed time in milliseconds

    static f64 now(); // Returns current time in seconds since epoch

private:
    std::chrono::high_resolution_clock::time_point startTime_;
};

class ScopedTimer {
public:
    ScopedTimer(const String& name);
    ~ScopedTimer();

private:
    String name_;
    Timer timer_;
};

}
