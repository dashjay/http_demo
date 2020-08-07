#include <iostream>
#include <thread>

void thread_sleep(int second) {
    std::this_thread::sleep_for(std::chrono::seconds(second));
    std::cout << "sleep over" << '\n';
}

int main() {
    {
        std::thread thr(thread_sleep, 5);
    }
//    thr.join();
    std::cout << "program exit" << '\n';
//
//    std::thread thr2(thread_sleep, 999);
//    thr2.detach();
//    std::cout << "program exit directly" << '\n';
}