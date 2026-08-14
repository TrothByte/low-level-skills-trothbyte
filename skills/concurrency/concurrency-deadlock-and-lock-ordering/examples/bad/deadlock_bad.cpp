#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex A, B;
std::atomic<bool> done1{false}, done2{false};

void worker1() {
    A.lock();
    std::cout << "worker1: locked A, waiting for B\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    B.lock();
    std::cout << "worker1: locked B\n";
    B.unlock();
    A.unlock();
    done1 = true;
}

void worker2() {
    B.lock();
    std::cout << "worker2: locked B, waiting for A\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    A.lock();
    std::cout << "worker2: locked A\n";
    A.unlock();
    B.unlock();
    done2 = true;
}

int main() {
    std::thread t1(worker1), t2(worker2);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    bool finished = done1.load() && done2.load();
    t1.detach();
    t2.detach();
    if (!finished) {
        std::cout << "WATCHDOG: ABBA DEADLOCK - threads did not finish within 3s\n";
        return 42;
    }
    std::cout << "threads finished\n";
    return 0;
}
