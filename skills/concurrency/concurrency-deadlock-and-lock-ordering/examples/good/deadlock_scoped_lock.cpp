#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex A, B;
std::atomic<int> count{0};

void worker() {
    for (int i = 0; i < 5; ++i) {
        std::scoped_lock<std::mutex, std::mutex> lock(A, B);
        count.fetch_add(1, std::memory_order_relaxed);
    }
}

int main() {
    std::thread t1(worker), t2(worker);
    t1.join();
    t2.join();
    int n = count.load();
    std::cout << "scoped_lock: count=" << n << " (expect 10)\n";
    return n == 10 ? 0 : 1;
}
