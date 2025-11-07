

#include "task.h"
#include "generator.h"

#include <chrono>
#include <thread>
#include <iostream>

using namespace std::chrono_literals;




generator<std::uint64_t> fib(unsigned int n) 
{
    if(n > 94)
        throw std::logic_error("too big of fibonacci sequence");

    if(n == 0) co_return;

    uint64_t a = 0;
    co_yield a;
    if(n == 1) co_return;

    uint64_t b = 1;
    co_yield b;
    if(n == 2) co_return;

    for(unsigned int i = 2; i < n; ++i)
    {
        std::uint64_t s = a + b;
        co_yield s;
        a = b;
        b = s;
    }
}

// Forward declaration of some other function. Its implementation is not relevant.
task<float> f(float x)
{
    std::this_thread::sleep_for(0.5s);
    co_return x + 5;
}

task<int> g(int x) 
{
    int fx = (int)co_await f(x);

    int y = 0;
    for(int f : fib(fx))
        y += f;

    co_return y;
}

// task<int> h(int n)
// {
//     std::cout << "co_awaiting a generator: ";

//     generator<uint64_t> g = fib(10);
//     for(;;)
//     {
//         std::optional<int> value = co_await g;
//         if(!value.has_value())
//             break;
            
//         std::cout << *value << ", ";
//         std::cout.flush();
//     }

//     co_return 0;
// }


int main(int ac, char * av[])
{
    // auto t = g(3);

    // std::cout << "task get: " << t.get() << std::endl;

    std::cout <<  "fib(10):\n";
    for(auto const & x : fib(10))
        std::cout << x << ", ";

    std::cout << std::endl;

    std::cout << "using operator bool and ()" << std::endl;
    auto f = fib(11);

    while(f)
        std::cout << f() << ", ";
    
    std::cout << std::endl;



    // auto u = h(10);

    // std::cout << u.get() << std::endl;

    // return 0;
}

