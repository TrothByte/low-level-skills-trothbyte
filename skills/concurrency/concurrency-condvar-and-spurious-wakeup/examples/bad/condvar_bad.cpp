#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex m;
std::condition_variable cv;
bool ready = false;

void producer() {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        std::lock_guard<std::mutex> lk(m);
        ready = true;
        std::cout << "producer: set ready=true, notifying\n";
    }
    cv.notify_one();
}

void consumer() {
    std::unique_lock<std::mutex> lk(m);
    std::cout << "consumer: entering wait WITHOUT predicate\n";
    cv.wait(lk);
    std::cout << "consumer: woken, ready=" << (ready ? "true" : "false") << "\n";
}

int main() {
    std::thread p(producer);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::thread c(consumer);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "WATCHDOG: consumer still waiting => LOST WAKEUP (notify arrived before wait)\n";
    p.detach();
    c.detach();
    return 42;
}
