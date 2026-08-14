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
    cv.wait(lk, [] { return ready; });
    std::cout << "consumer: woke with ready=true\n";
}

int main() {
    std::thread p(producer);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::thread c(consumer);
    c.join();
    p.join();
    std::cout << "done: predicate wait survives the lost-wakeup timing\n";
    return 0;
}
