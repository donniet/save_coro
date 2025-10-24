

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
    { 
        m_value = std::move(value); 
        m_ready = true;
    }

    void unhandled_exception() { throw std::current_exception(); }

    operator return_type() const
    { return m_value; }

    return_type const & get() const
    { return m_value; }

    bool is_ready() const 
    { return m_ready; }

    no_yield_promise() : m_ready{false} { }

private:
    return_type m_value;
    bool m_ready;
};

template<typename T, typename InitialAwaiter>
class no_yield_coroutine
{
public:
    using return_type = T;
    using promise_type = no_yield_promise<T, InitialAwaiter>;

    friend struct no_yield_promise<T, InitialAwaiter>;

    promise_type & promise()
    { return handle().promise(); }

    promise_type const & promise() const 
    { return handle().promise(); }

    std::coroutine_handle<promise_type> handle() const
    { return std::coroutine_handle<promise_type>::from_address(m_address); }

    void resume() { handle().resume(); }
    void operator()() { resume(); }

    bool is_ready() const 
    { return promise().is_ready(); }

    return_type const & get() const
    { return promise().get(); }

    operator return_type() const
    { return promise().get(); }

    // awaiter methods
    bool await_ready() 
    { return is_ready(); }

    template<typename Promise>
    auto await_suspend(std::coroutine_handle<Promise> const & handle)
    {
        resume();
        return handle;
    }   

    T await_resume()
    { return get(); }

    no_yield_coroutine() : m_address{nullptr} { }
    no_yield_coroutine(no_yield_coroutine const &) = default;
    no_yield_coroutine(no_yield_coroutine &&) = default;
    // no_yield_coroutine(void * address) : 
    //     no_yield_coroutine(std::coroutine_handle<promise_type>::from_address(address)) 
    // { }
    no_yield_coroutine & operator=(no_yield_coroutine const &) = default;
    no_yield_coroutine & operator=(no_yield_coroutine &&) = default;
    no_yield_coroutine & operator=(std::nullptr_t)
    {
        m_address = nullptr;
        return *this;
    }

    no_yield_coroutine(std::coroutine_handle<promise_type> handle) :
        m_address(handle.address())
    { }

    void * m_address;
};

template<typename T, typename InitialAwaiter>
no_yield_coroutine<T, InitialAwaiter> no_yield_promise<T, InitialAwaiter>::get_return_object()
{ return { std::coroutine_handle<no_yield_promise<T, InitialAwaiter>>::from_promise(*this) }; }

template<typename T>
using eager = no_yield_coroutine<T, std::suspend_never>;

template<typename T>
using lazy = no_yield_coroutine<T, std::suspend_always>;


saveable<lazy<int>> bar(int y)
{
    co_return y * 3;
}

saveable<lazy<int>> run(int * x)
{
    int y = co_await bar(*x);

    co_return y + 5;
}
 

int main(int ac, char * av[])
{
    int x = 3;
    int y = 27;

    auto coro = run(&x);

    std::ofstream ofs("saved.coro" );
    coro.save(ofs);
    ofs.close();

    // coro.destroy();

    std::ifstream ifs("saved.coro");
    auto coro2 = load_coro<lazy<int>>(ifs, &y);
    ifs.close();


    coro2.handle().resume();
    std::cerr << "updated value: " << std::dec << coro2.handle().get() << std::endl;

    coro.handle().resume();
    std::cerr << "original value: " << coro.handle().get() << std::endl;


    return 0;
}