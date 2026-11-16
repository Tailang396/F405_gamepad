#include <chrono>
#include <thread>

extern "C" void demo_delay_ms(unsigned milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
