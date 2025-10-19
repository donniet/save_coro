
#include <coroutine>
#include <iostream>
#include <generator>
#include <fstream>


using version_t = unsigned long;

static constexpr inline version_t saveable_coroutine_version = 0x00'00'0001;


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



// this is another attempt to simplify the template of the handle
// and keep the harness into the coroutine through the arguments

struct frame_header {
    size_t size;
    size_t data_size;
    size_t offsets;
    version_t version;
    size_t hash_code;
};

template<typename HandleType, typename... ArgTypes>
struct promise_type;

template<>
struct promise_type<void>
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
};

template<typename HandleType>
struct saveable {
    void save(std::ostream & os)
    {
        frame_header * header = promise_type<void>::header_from(m_address);

        // write out the frame including the size header
        os.write(reinterpret_cast<char*>(promise_type<void>::frame_start(m_address)), header->size);
    }

    HandleType handle() { return m_handle; }

    saveable(void * address) : 
        m_address(address), m_handle(address) 
    {  }

    template<typename... ArgTypes>
    saveable(std::coroutine_handle<promise_type<HandleType, ArgTypes...>> h) :
        m_address(h.address()), m_handle(h.address())
    { }

    void * m_address;
    HandleType m_handle;
};


template<typename HandleType, typename... ArgTypes>
struct promise_type : 
    public std::coroutine_traits<HandleType, ArgTypes...>::promise_type 
{
    struct frame_footer {
        long argument_offset[sizeof...(ArgTypes)];
    };

    static void * operator new(size_t size)
    {
        //                  | header             | data | footer              |
        size_t frame_size = sizeof(frame_header) + size + sizeof(frame_footer);
        void * mem = ::operator new(frame_size);

        *reinterpret_cast<frame_header*>(mem) = {
            .size = frame_size,
            .data_size = size,
            .offsets = sizeof...(ArgTypes), 
            .version = saveable_coroutine_version,
            .hash_code = typeid(promise_type).hash_code(),
        };

        std::cerr << "promise_type new(size): " << std::hex << reinterpret_cast<void*>(reinterpret_cast<char*>(mem) + sizeof(frame_header)) << std::endl;
        return reinterpret_cast<char*>(mem) + sizeof(frame_header);
    }

    static void * operator new(size_t size, frame_header const & header)
    {
        //                  | header             | data | footer              |
        size_t frame_size = sizeof(frame_header) + header.data_size + sizeof(frame_footer);
        void * mem = ::operator new(frame_size);

        *reinterpret_cast<frame_header*>(mem) = header;

        std::cerr << "promise_type new(size, header): " << std::hex << reinterpret_cast<void*>(reinterpret_cast<char*>(mem) + sizeof(frame_header)) << std::endl;
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

    static frame_footer * footer_from(void * address)
    {
        return reinterpret_cast<frame_footer*>(
            reinterpret_cast<char*>(address) + header_from(address)->data_size
        );
    }

    promise_type(ArgTypes&... args) : // connects the upgradeables through the coroutine
        std::coroutine_traits<HandleType, ArgTypes...>::promise_type{}
    {
        void * address = std::coroutine_handle<promise_type>::from_promise(*this).address();
        frame_footer * footer = footer_from(address);

        int i = 0;
        ( ( footer->argument_offset[i++] = (reinterpret_cast<char*>(&args) - reinterpret_cast<char*>(address)) ), ... );

        std::cerr << "promise_type coroutine construtor" << std::endl;
    } 
    
    
    promise_type(frame_header * header) :        // creates a promise from a saved handle
        std::coroutine_traits<HandleType, ArgTypes...>::promise_type{}
    { 
        std::cerr << "promise_type header constructor" << std::endl;
    }
    
    promise_type() : 
        std::coroutine_traits<HandleType, ArgTypes...>::promise_type{}
    { 
        std::cerr << "promise_type default constructor" << std::endl;
    }

    //...
    saveable<HandleType> get_return_object()
    { return { std::coroutine_handle<promise_type>::from_promise(*this) }; }

    ~promise_type()
    { std::cerr << "promise_type destructor: " << std::hex << std::coroutine_handle<promise_type>::from_promise(*this).address() << std::endl; }

};

template<typename HandleType, typename... ArgTypes>
saveable<HandleType> load_coro(std::istream & is, ArgTypes const &... args)
{
    using promise_type = promise_type<HandleType, ArgTypes...>;

    // read in the header
    frame_header header;
    is.read(reinterpret_cast<char*>(&header), sizeof(frame_header));

    // check the version and hash code
    if(header.version != saveable_coroutine_version)
        throw std::logic_error("version mismatch");

    if(header.hash_code != typeid(promise_type).hash_code())
        throw std::logic_error("hash_code mismatch");
    
    // allocate the memory for the frame using the dedicated allocator
    void * address = new(header) promise_type(&header); //, args...);

    // read in the rest of the frame into allocated memory
    is.read(reinterpret_cast<char*>(address), header.data_size);

    // read in the footer
    typename promise_type::frame_footer footer;
    is.read(reinterpret_cast<char*>(&footer), sizeof(footer));

    // copy the arguments to the frame using the offsets
    int i = 0;
    ( ( *reinterpret_cast<ArgTypes*>(reinterpret_cast<char*>(address) + footer.argument_offset[i++]) = args ), ... );

    return { address };
}

namespace std {
    // promise_type for saveable with arguments to coroutine
    template<typename HandleType, typename... ArgTypes>
    struct coroutine_traits<saveable<HandleType>, ArgTypes...> {
        using promise_type = promise_type<HandleType, ArgTypes...>;
    };
}

saveable<lazy<int>> run(int * x)
{
    co_return *x;
}
 

int main(int ac, char * av[])
{
    int x = 3;
    int y = 27;

    auto coro = run(&x);

    std::ofstream ofs("saved.coro" );
    coro.save(ofs);
    ofs.close();

    std::ifstream ifs("saved.coro");
    auto coro2 = load_coro<lazy<int>>(ifs, &y);
    ifs.close();

    coro2.handle().resume();
    std::cerr << "updated value: " << std::dec << coro2.handle().get() << std::endl;

    coro.handle().resume();
    std::cerr << "original value: " << coro.handle().get() << std::endl;

    return 0;
}