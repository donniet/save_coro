#ifndef __SAVEABLE_COROUTINE_H__
#define __SAVEABLE_COROUTINE_H__


#include <coroutine>
#include <iostream>

using std::size_t;

using version_t = unsigned long;

static constexpr inline version_t saveable_coroutine_version = 0x00'00'0001;

struct frame_header {
    size_t size;
    size_t data_size;
    version_t version;
    size_t hash_code;

    union {
        void * child_address;
        size_t frame_count;
    };
};

template<typename HandleType, typename... ArgTypes>
struct saveable_promise;

// template<typename Awaitable>
// struct awaitable_reference
// {
//     Awaitable * m_awaitable;

//     auto await_ready() const 
//     { return m_awaitable->await_ready(); }

//     template<typename Promise>
//     auto await_suspend(std::coroutine_handle<Promise> handle)
//     { return m_awaitable->await_suspend(handle); }

//     auto await_resume() const
//     { return m_awaitable->await_resume(); }

//     awaitable_reference(Awaitable & awaitable) :
//         m_awaitable(&awaitable)
//     { }
// };

template<>
struct saveable_promise<void>
{
    static frame_header * header_from(void * address) 
    { return reinterpret_cast<frame_header*>(frame_start(address)); }

    static void * frame_start(void * address)
    { return reinterpret_cast<char*>(address) - sizeof(frame_header); }

    static void operator delete(void * addr)
    {
        void * mem = reinterpret_cast<char*>(addr) - sizeof(frame_header);
        ::operator delete(mem);
    }

    static void * operator new(size_t size, frame_header const & header)
    {
        //                  | header             | data 
        size_t frame_size = sizeof(frame_header) + header.data_size;
        void * mem = ::operator new(frame_size);

        *reinterpret_cast<frame_header*>(mem) = header;

        std::cerr << "saveable_promise new(size, header): " << std::hex << reinterpret_cast<void*>(reinterpret_cast<char*>(mem) + sizeof(frame_header)) << std::endl;
        return reinterpret_cast<char*>(mem) + sizeof(frame_header);
    }
};

struct saveable_base
{
    frame_header * get_header()
    { return saveable_promise<void>::header_from(m_address); }

    void save(std::ostream & os)
    {
        size_t frame_count = 1;
        frame_header * header = get_header();

        for(frame_header * h = header; h->child_address != nullptr; ++frame_count)
            h = saveable_promise<void>::header_from(h->child_address);

        // first write out the number of frames
        os.write(reinterpret_cast<char*>(&frame_count), sizeof(size_t));

        frame_header wh;
        
        // the write out all the frames
        for(void * addr = m_address; ; --frame_count)
        {
            wh = *header;
            // wh.child_address = nullptr;
            wh.frame_count = frame_count;

            // write out the header
            os.write(reinterpret_cast<char*>(&wh), sizeof(frame_header));
            
            // write out the data
            os.write(reinterpret_cast<char*>(addr), header->data_size);

            addr = header->child_address;
            if(addr == nullptr)
                break;

            header = saveable_promise<void>::header_from(header->child_address);
        }
    }

    saveable_base(void * address) : m_address{address} { }

    void * address() const 
    { return m_address; }

    void * m_address;
};

template<typename HandleType>
struct saveable;

template<typename HandleType>
struct saveable : public saveable_base {
    using wrapped_promise_type = std::coroutine_traits<HandleType>::promise_type;
    using wrapped_cohandle_type = std::coroutine_handle<wrapped_promise_type>;

    // destroys the coroutine frame
    void destroy()
    {   
        std::coroutine_handle<>::from_address(m_address).destroy();
        m_address = nullptr;
        m_handle = nullptr;
    }

    HandleType handle() { return m_handle; }


    // how to handle the operator co_await() 
    bool await_ready() { return m_handle.await_ready(); }

    template<typename HandleType2>
    auto await_suspend(HandleType2 handle) 
    { return m_handle.await_suspend(handle); }

    auto await_resume()
    { return m_handle.await_resume(); }

    saveable(void * address) : 
        saveable_base{address}, 
        m_handle{wrapped_cohandle_type::from_address(address)}
    {  }


    template<typename... ArgTypes>
    saveable(std::coroutine_handle<saveable_promise<HandleType, ArgTypes...>> h, 
             HandleType const & handle) :
        saveable_base{h.address()}, m_handle{handle}
    { }

    HandleType m_handle;
};


struct suspend_aware
{
    virtual void set_handle(std::coroutine_handle<>) = 0;
};

template<typename HandleType, typename... ArgTypes>
struct saveable_promise : 
    public std::coroutine_traits<HandleType, ArgTypes...>::promise_type 
{
    template<size_t I>
    struct argument_traits
    {
        using type = std::tuple_element_t<I, std::tuple<ArgTypes...>>;
    };

    static void * operator new(size_t size)
    {
        //                  | header             | data 
        size_t frame_size = sizeof(frame_header) + size;
        void * mem = ::operator new(frame_size);

        *reinterpret_cast<frame_header*>(mem) = {
            .size = frame_size,
            .data_size = size,
            .version = saveable_coroutine_version,
            .hash_code = typeid(saveable_promise).hash_code(),
            .child_address = nullptr,
        };

        std::cerr << "saveable_promise new(size): " << std::hex << reinterpret_cast<void*>(reinterpret_cast<char*>(mem) + sizeof(frame_header)) << std::endl;
        return reinterpret_cast<char*>(mem) + sizeof(frame_header);
    }

    static void operator delete(void * addr)
    {
        void * mem = reinterpret_cast<char*>(addr) - sizeof(frame_header);
        ::operator delete(mem);

        std::cerr << "promise_type delete: " << std::hex << addr << std::endl;
    }

    static frame_header * header_from(void * address) 
    {
        return reinterpret_cast<frame_header*>(
            reinterpret_cast<char*>(address) - sizeof(frame_header)
        );
    }

    template<typename HandleType2>
    auto await_transform(saveable<HandleType2> child_handle) 
    {
        auto cohandle = 
            std::coroutine_handle<saveable_promise<HandleType, ArgTypes...>>::from_promise(*this);

        void * address = cohandle.address();
        frame_header * header = header_from(address);

        header->child_address = child_handle.address();

        return child_handle;
    }

    template<size_t I>
    void hydrate_argument(argument_traits<I>::type * arg)
    {
        auto cohandle = std::coroutine_handle<saveable_promise>::from_promise(*this);
        void * address = cohandle.address();

        // does this argument refer to any coroutine handles?
        // if so replace it with the approprate handle to the hydrated coroutine

        auto parg = reinterpret_cast<argument_traits<I>::type*>(
            reinterpret_cast<char*>(address) + m_argument_offset[I]);

        if constexpr(std::is_base_of<suspend_aware, typename argument_traits<I>::type>::value)
        {
            if(m_suspended)
                dynamic_cast<suspend_aware*>(arg)->set_handle(cohandle);
            else
                dynamic_cast<suspend_aware*>(arg)->set_handle(nullptr);
        }

        *parg = *arg;
    }


    void hydrate_arguments(ArgTypes & ... args)
    { hydrate_arguments(std::make_tuple<ArgTypes*...>(&args...), std::make_index_sequence<sizeof...(ArgTypes)>{}); }

    template<size_t ... Is>
    void hydrate_arguments(std::tuple<ArgTypes*...> args, std::index_sequence<Is...>)
    {
        ( hydrate_argument<Is>(std::get<Is>(args)), ... );
    }

    // we need a way to keep track of what this routine is waiting for
    // 
    template<typename Awaitable>
    struct waiting_on {
        bool await_ready()
        { return m_awaitable.await_ready(); }
    
        auto await_suspend(std::coroutine_handle<saveable_promise<HandleType, ArgTypes...>> handle)
        {
            // here we can monitor if the coroutine is suspended
            // and the handle that we are suspended on

            // after hydration we can replace this handle in a 
            // hydration aware argument
            auto & promise = handle.promise();
            promise.m_suspended = true;
            m_handle = handle;
            return m_awaitable.await_suspend(handle);
        }

        auto await_resume()
        { 
            // here we can clear the state of awaiting 
            auto & promise = m_handle.promise();
            promise.m_suspended = false;
            m_handle = nullptr;
            return m_awaitable.await_resume(); 
        }

        Awaitable m_awaitable;
        std::coroutine_handle<saveable_promise<HandleType, ArgTypes...>> m_handle;
    };

    // return non-savables directly
    // TODO: create concept for awaitables that are not saveable
    template<typename Awaitable>
    auto await_transform(Awaitable & awaitable)
    // { return awaitable_reference<Awaitable>(awaitable); }
    // { return awaitable.operator co_await(); }
    { return waiting_on{awaitable, nullptr}; }

    saveable_promise(ArgTypes&... args) : // connects the upgradeables through the coroutine
        std::coroutine_traits<HandleType, ArgTypes...>::promise_type{},
        m_suspended{false}
    {
        void * address = std::coroutine_handle<saveable_promise>::from_promise(*this).address();

        int i = 0;
        ( ( m_argument_offset[i++] = (reinterpret_cast<char*>(&args) - reinterpret_cast<char*>(address)) ), ... );

        std::cerr << "promise_type coroutine construtor" << std::endl;
    } 
    
    
    saveable_promise(frame_header * header) :        // creates a promise from a saved handle
        std::coroutine_traits<HandleType, ArgTypes...>::promise_type{},
        m_suspended{false}
    { 
        std::cerr << "promise_type header constructor" << std::endl;
    }
    
    saveable_promise() : 
        std::coroutine_traits<HandleType, ArgTypes...>::promise_type{},
        m_suspended{false}
    { 
        std::cerr << "promise_type default constructor" << std::endl;
    }

    //...
    saveable<HandleType> get_return_object()
    { return { 
        std::coroutine_handle<saveable_promise>::from_promise(*this),
        std::coroutine_traits<HandleType, ArgTypes...>::promise_type::get_return_object(),
    }; }

    ~saveable_promise()
    { std::cerr << "promise_type destructor: " << std::hex << std::coroutine_handle<saveable_promise>::from_promise(*this).address() << std::endl; }

    long m_argument_offset[sizeof...(ArgTypes)];
    bool m_suspended;
};

template<typename HandleType, typename... ArgTypes>
saveable<HandleType> load_coro(std::istream & is, ArgTypes  &... args)
{
    using promise_type = saveable_promise<HandleType, ArgTypes...>;

    // first read in the number of frames
    size_t frame_count;
    is.read(reinterpret_cast<char*>(&frame_count), sizeof(size_t));

    frame_header * prev_header = nullptr;
    void * return_address = nullptr;
    frame_header header;

    for(; frame_count > 0; --frame_count)
    {
        // read in the header
        is.read(reinterpret_cast<char*>(&header), sizeof(frame_header));

        // if this is the first header, check the version and hash
        if(return_address == nullptr) 
        {
            // check the version and hash code
            if(header.version != saveable_coroutine_version)
                throw std::logic_error("version mismatch");

            if(header.hash_code != typeid(promise_type).hash_code())
                throw std::logic_error("hash_code mismatch");
        }
        
        // allocate the memory for the frame using the dedicated allocator
        void * address = 
            saveable_promise<void>::operator new(header.data_size, header); //, args...);

        // read in the rest of the frame into allocated memory
        is.read(reinterpret_cast<char*>(address), header.data_size);

        // if this is the first header update the args
        if(return_address == nullptr)
        {
            auto cohandle = 
                std::coroutine_handle<promise_type>::from_address(address);

            promise_type & promise = cohandle.promise();

            promise.hydrate_arguments(args...);

            return_address = address;
        }
        // otherwise set the previous frame's child pointer
        else
        {
            prev_header->child_address = address;
        }

        prev_header = saveable_promise<void>::header_from(address);
    }

    return { return_address };
}

namespace std {
    // promise_type for saveable with arguments to coroutine
    template<typename HandleType, typename... ArgTypes>
    struct coroutine_traits<saveable<HandleType>, ArgTypes...> {
        using promise_type = saveable_promise<HandleType, ArgTypes...>;
    };
}


#endif