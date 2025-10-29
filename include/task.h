#ifndef __TASK_H__
#define __TASK_H__

#include <coroutine>
#include <variant>
#include <stdexcept>
#include <future>
#include <iostream>

template<typename T>
class task {
public:
    struct awaiter;

    class promise_type {
    public:
        promise_type() noexcept;
        ~promise_type();

        struct final_awaiter {
            bool await_ready() noexcept;
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept;
            void await_resume() noexcept;
        };

        // how can we extract the return value via the task returned here?
        task get_return_object() noexcept;
        std::suspend_always initial_suspend() noexcept;
        final_awaiter final_suspend() noexcept;
        void unhandled_exception() noexcept;
        void return_value(T && result) noexcept;
        void return_value(T const & result) noexcept;

        // int get();

    private:
        friend task;
        friend task::awaiter;
        friend final_awaiter;

        std::coroutine_handle<> continuation_;
        std::variant<std::monostate, T, std::exception_ptr> result_;
    };

    task(task&& t) noexcept;
    ~task();
    task& operator=(task&& t) noexcept;

    bool done();
    T get();

    struct awaiter {
        explicit awaiter(std::coroutine_handle<promise_type> h) noexcept;
        bool await_ready() noexcept;
        std::coroutine_handle<promise_type> await_suspend(
            std::coroutine_handle<> h) noexcept;
        T await_resume();
    private:
        std::coroutine_handle<promise_type> coro_;
    };

    awaiter operator co_await() && noexcept;

private:
    explicit task(std::coroutine_handle<promise_type> h) noexcept;

    std::coroutine_handle<promise_type> coro_;
};

/**
 * task implementation
 */
template<typename T>
bool task<T>::done() 
{ return coro_.done(); }

// if using the task outside of a coroutine, this method
// will retrieve the value
template<typename T>
T task<T>::get()
{
    using std::holds_alternative, std::exception_ptr,
        std::rethrow_exception;

    if(!done())
        coro_.resume();

    auto & p = coro_.promise();

    if(holds_alternative<T>(p.result_))
        return std::get<T>(p.result_);

    if(holds_alternative<exception_ptr>(p.result_))
        rethrow_exception(
            std::get<exception_ptr>(p.result_));

    throw std::logic_error("incomplete task");
}

// if we co_await a task return the awaiter
template<typename T>
task<T>::awaiter task<T>::operator co_await() && noexcept
{ return awaiter{coro_}; }

template<typename T>
task<T>::task(std::coroutine_handle<promise_type> h) noexcept :
    coro_{h}
{ }

template<typename T>
task<T>::task(task&& t) noexcept : coro_{std::move(t.coro_)}
{ t.coro_ = nullptr; }

template<typename T>
task<T>& task<T>::operator=(task<T>&& t) noexcept
{ 
    if(this == &t)
        return *this;
    coro_ = std::move(t.coro_);
    t.coro_ = nullptr;
    return *this;
}

// if the task is destoyed then also destroy the coroutine
template<typename T>
task<T>::~task()
{ coro_.destroy(); }


/***

template<typename T>* task<T>::awaiter implementation
 */
template<typename T>
task<T>::awaiter::awaiter(std::coroutine_handle<promise_type> h) noexcept :
    coro_{h}
{ }

// we are ready (no suspend) if our promise holds something other
// than std::monostate
template<typename T>
bool task<T>::awaiter::await_ready() noexcept
{ return !std::holds_alternative<std::monostate>(
    coro_.promise().result_); }

// on suspend, store the caller coroutine handle as a continuation
// we will continue that coroutine in the final_awaiter
template<typename T>
std::coroutine_handle<typename task<T>::promise_type> 
task<T>::awaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    coro_.promise().continuation_ = h;
    return coro_;
}

template<typename T>
T task<T>::awaiter::await_resume()
{
    using std::holds_alternative, std::exception_ptr, std::rethrow_exception;
    auto & p = coro_.promise();
    // if we are storing an exception_ptr, rethrow it
    if(holds_alternative<exception_ptr>(p.result_))
        rethrow_exception(std::get<exception_ptr>(p.result_));
    // otherwise return the result
    return std::get<T>(p.result_);
}


/***

template<typename T>* task<T>::promise_type implementation
 */
template<typename T>
task<T>::promise_type::promise_type() noexcept : 
    continuation_{}, result_{}
{ 
    // std::cerr << "\npromise_type[" << std::hex << this << std::dec << "]" << std::endl;
}

template<typename T>
task<T>::promise_type::~promise_type()
{ 
    // std::cerr << "\n~promise_type[" << std::hex << this << std::dec << "]" << std::endl;
}

template<typename T>
std::suspend_always task<T>::promise_type::initial_suspend() noexcept
{ return {}; }

template<typename T>
task<T> task<T>::promise_type::get_return_object() noexcept
{ return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }

template<typename T>
void task<T>::promise_type::return_value(T const& result) noexcept
{ result_ = result; }

template<typename T>
void task<T>::promise_type::return_value(T && result) noexcept
{ result_ = std::move(result); }

template<typename T>
void task<T>::promise_type::unhandled_exception() noexcept
{ result_ = std::current_exception(); }

template<typename T>
task<T>::promise_type::final_awaiter task<T>::promise_type::final_suspend() noexcept
{ return {}; }


/***

template<typename T>* task<T>::promise_type::final_awaiter implementation
 */
template<typename T>
bool task<T>::promise_type::final_awaiter::await_ready() noexcept
{ return false; } // suspend this coroutine after execution

template<typename T>
std::coroutine_handle<> task<T>::promise_type::final_awaiter::await_suspend(
    std::coroutine_handle<promise_type> h) noexcept
{
    // if we have a continuation, resume it by returning it
    if(h.promise().continuation_)   
        return h.promise().continuation_;
    
    // otherwise return a noop to end the chain of coroutines
    return std::noop_coroutine(); // so that's what noop coroutines are for...
}

template<typename T>
void task<T>::promise_type::final_awaiter::await_resume() noexcept
{ }



#endif
