#include "Timer.h"

namespace WorldEditor {

Timer::Timer() {
    reset();
}

void Timer::reset() {
    startTime_ = std::chrono::high_resolution_clock::now();
}

f64 Timer::elapsed() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime_);
    return static_cast<f64>(duration.count()) / 1e9;
}

f64 Timer::elapsedMillis() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime_);
    return static_cast<f64>(duration.count()) / 1e6;
}

f64 Timer::now() {
    auto time = std::chrono::high_resolution_clock::now();
    auto duration = time.time_since_epoch();
    return static_cast<f64>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()) / 1e9;
}

ScopedTimer::ScopedTimer(const String& name) : name_(name) {
    LOG_DEBUG("Starting timer: {}", name_);
}

ScopedTimer::~ScopedTimer() {
    f64 elapsed = timer_.elapsedMillis();
    LOG_DEBUG("Timer '{}' finished in {:.2f} ms", name_, elapsed);
}

}
