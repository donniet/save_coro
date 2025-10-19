

#include "saveable_coroutine.h"

#include <coroutine>
#include <iostream>
#include <generator>
#include <fstream>
#include <sstream>



template<typename T, typename InitialAwaiter = std::suspend_never>
class no_yield_coroutine;

template<typename T, typename InitialAwaiter = std::suspend_never>
struct no_yield_promise 
{
    using return_type = T;

    InitialAwaiter initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    no_yield_coroutine<T, InitialAwaiter> get_return_object();

    void return_value(return_type value)
    { m_value = std::move(value); }

    void unhandled_exception() { throw std::current_exception(); }

    operator return_type() const
    { return m_value; }

    return_type const & get() const
    { return m_value; }

private:
    return_type m_value; 
};

template<typename T, typename InitialAwaiter>
class no_yield_coroutine : 
    public std::coroutine_handle<no_yield_promise<T, InitialAwaiter>>,
    public InitialAwaiter
{
public:
    using return_type = T;
    using promise_type = no_yield_promise<T, InitialAwaiter>;

    friend struct no_yield_promise<T, InitialAwaiter>;

    return_type const & get() const
    { return std::coroutine_handle<promise_type>::promise().get(); }

    operator return_type() const
    { return std::coroutine_handle<promise_type>::promise().get(); }

    no_yield_coroutine() { }
    no_yield_coroutine(no_yield_coroutine const &) = default;
    no_yield_coroutine(no_yield_coroutine &&) = default;
    no_yield_coroutine(void * address) : 
        no_yield_coroutine(std::coroutine_handle<promise_type>::from_address(address)) 
    { }
    no_yield_coroutine & operator=(no_yield_coroutine const &) = default;
    no_yield_coroutine & operator=(no_yield_coroutine &&) = default;
    

protected:
    no_yield_coroutine(std::coroutine_handle<promise_type> handle) :
        std::coroutine_handle<promise_type>(handle)
    { }
};

template<typename T, typename InitialAwaiter>
no_yield_coroutine<T, InitialAwaiter> no_yield_promise<T, InitialAwaiter>::get_return_object()
{ return { std::coroutine_handle<no_yield_promise<T, InitialAwaiter>>::from_promise(*this) }; }

template<typename T>
using eager = no_yield_coroutine<T, std::suspend_never>;

template<typename T>
using lazy = no_yield_coroutine<T, std::suspend_always>;


saveable<lazy<int>> run(int * x)
{
    co_return *x + 5;
}
 

int main(int ac, char * av[])
{
    int x = 3;
    int y = 27;

    auto coro = run(&x);

    std::ofstream ofs("saved.coro" );
    coro.save(ofs);
    ofs.close();

    coro.destroy();

    std::ifstream ifs("saved.coro");
    auto coro2 = load_coro<lazy<int>>(ifs, &y);
    ifs.close();


    coro2.handle().resume();
    std::cerr << "updated value: " << std::dec << coro2.handle().get() << std::endl;

    // coro.handle().resume();
    // std::cerr << "original value: " << coro.handle().get() << std::endl;


    return 0;
}