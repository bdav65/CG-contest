#pragma once

#include <chrono>


class Timer {
    public:
        void log(std::string message, bool reset = true);
        void log(char const *message, bool reset = true);
        std::chrono::milliseconds get();
        Timer();

    private:
        std::chrono::high_resolution_clock::time_point start;
};
