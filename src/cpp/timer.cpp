#include "timer.hpp"

#include <iostream>


Timer::Timer():
    start(std::chrono::high_resolution_clock::now())
{}


std::chrono::milliseconds Timer::get() {
    auto current = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(current - start);
}


void Timer::log(std::string message, bool reset) {
    log(message.c_str(), reset);
}


void Timer::log(char const *message, bool reset) {
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    std::cout << "log: " << message << " (" << duration << ")\n";

    if (reset) {
        start = stop;
    }
}
