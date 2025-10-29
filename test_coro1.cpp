

#include "task.h"

#include <chrono>
#include <thread>
#include <iostream>

using namespace std::chrono_literals;

// Forward declaration of some other function. Its implementation is not relevant.
task<float> f(float x)
{
    std::this_thread::sleep_for(0.5s);
    co_return x + 5;
}

task<int> g(int x) {
    int fx = (int)co_await f(x);
    co_return fx * fx;
}

int main(int ac, char * av[])
{
    auto t = g(3);

    std::cout << "task get: " << t.get() << std::endl;

    return 0;
}

