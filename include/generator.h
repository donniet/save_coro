#ifndef __GENERATOR_H__
#define __GENERATOR_H__

#include <coroutine>
#include <variant>
#include <stdexcept>
#include <optional>

template<typename T>
class generator {
public:
    class promise_type {
    public:
        promise_type() noexcept;
        ~promise_type();

        generator get_return_object() noexcept;
        std::suspend_always initial_suspend() noexcept;
        std::suspend_always final_suspend() noexcept;
        void unhandled_exception() noexcept;
        void return_void() noexcept;

        template<std::convertible_to<T> From>
        std::suspend_always yield_value(From&&) noexcept;

    private:
        friend generator;

        std::variant<std::monostate, T, std::exception_ptr> result_;
    };

    bool done();
    T next();

    // primary access methods
    operator bool(); // is not done
    T operator()();  // last yielded value and resume

    // publicly move constructable 
    generator(generator&& g) noexcept;
    generator& operator=(generator&& g) noexcept;
    ~generator();

    // to handle range iteration
    struct iterator
    {
        T operator*() const;
        // no operator-> since calling one would also resume the coroutine
        // and invalidate any further pointer access
        iterator& operator++();
        iterator& operator=(iterator const&);

        bool operator==(std::default_sentinel_t) const;
        bool operator!=(std::default_sentinel_t) const;

        generator<T>* gen_;

        explicit iterator(generator<T>*);
    };

    iterator begin();
    std::default_sentinel_t end();

private:
    // privately constructable by friend promise_type in get_return_object
    explicit generator(std::coroutine_handle<promise_type>) noexcept;

    // potentially idempotent method to resume coroutine if we have consumed
    // a value
    void fill();

    std::coroutine_handle<promise_type> coro_;
    bool full_;
};

/***
 * promise_type
 */
template<typename T>
generator<T> generator<T>::promise_type::get_return_object() noexcept
{ return generator<T>{
    std::coroutine_handle<promise_type>::from_promise(*this)}; }

template<typename T>
std::suspend_always generator<T>::promise_type::initial_suspend() noexcept
{ return {}; }

template<typename T>
void generator<T>::promise_type::unhandled_exception() noexcept
{ result_ = std::current_exception(); }

template<typename T>
void generator<T>::promise_type::return_void() noexcept
{ }

template<typename T>
template<std::convertible_to<T> From>
std::suspend_always
generator<T>::promise_type::yield_value(From&& from) noexcept
{ 
    result_ = std::forward<From>(from);
    return {};
}

template<typename T>
std::suspend_always
generator<T>::promise_type::final_suspend() noexcept
{ return {}; }

template<typename T>
generator<T>::promise_type::promise_type() noexcept :
    result_{}
{ 
    // std::cerr << "\ngenerator<T>::promise_type[" << std::hex << this << std::dec << "]" << std::endl;
}

template<typename T>
generator<T>::promise_type::~promise_type()
{ 
    // std::cerr << "\ngenerator<T>::~promise_type[" << std::hex << this << std::dec << "]" << std::endl;
}

/***
 * generator
 */
template<typename T>
generator<T>::generator(generator<T>&& g) noexcept :
    coro_{g.coro_}, full_{g.full_}
{ 
    g.coro_ = nullptr;
    g.full_ = false;
}

template<typename T>
generator<T>::generator(std::coroutine_handle<promise_type> h) noexcept :
    coro_{h}, full_{false}
{ }

template<typename T>
generator<T>& generator<T>::operator=(generator<T>&& g) noexcept
{
    if(this == &g)
        return *this;
    coro_ = g.coro_;
    full_ = g.full_;
    g.coro_ = nullptr;
    g.full_ = false;
    return *this;
}

template<typename T>
bool generator<T>::done()
{ 
    fill();
    return coro_.done();
}

template<typename T>
generator<T>::operator bool()
{ return !done(); } // valid

template<typename T>
T generator<T>::next()
{
    using std::holds_alternative, std::exception_ptr, std::rethrow_exception;
    fill();

    auto& p = coro_.promise();
    if(holds_alternative<exception_ptr>(p.result_))
        rethrow_exception(std::get<exception_ptr>(p.result_));

    full_ = false;
    return std::move(std::get<T>(p.result_));
}

template<typename T>
T generator<T>::operator()()
{ return next(); }

template<typename T>
void generator<T>::fill()
{
    if(full_)
        return;

    coro_.resume();
    full_ = true;
}

template<typename T>
generator<T>::iterator generator<T>::begin()
{ return iterator{this}; }

template<typename T>
std::default_sentinel_t generator<T>::end()
{ return std::default_sentinel; }

template<typename T>
generator<T>::~generator() 
{ coro_.destroy(); }

/*** 
 * iterator
 */
template<typename T>
generator<T>::iterator::iterator(generator<T>* gen) : 
    gen_{gen}
{ }

template<typename T>
T generator<T>::iterator::operator*() const
{ return (*gen_)(); }


template<typename T>
generator<T>::iterator& generator<T>::iterator::operator++()
{ return *this; }

template<typename T>
generator<T>::iterator& 
generator<T>::iterator::operator=(generator<T>::iterator const& other)
{
    if(this == &other)
        return *this;
    gen_ = other.gen_;
    return *this;
}

template<typename T>
bool generator<T>::iterator::operator==(std::default_sentinel_t) const
{ return gen_ == nullptr || !*gen_; } // 

template<typename T>
bool generator<T>::iterator::operator!=(std::default_sentinel_t) const
{ return gen_ != nullptr && *gen_; }


#endif