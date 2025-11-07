#ifndef __RING_HPP__
#define __RING_HPP__

#include <ranges>

template<typename const_iterator_begin, typename const_iterator_end>
class Ring 
{
public:
    auto begin() const { return begin_; }
    auto end() const { return end_; }

    using const_iterator = const_iterator_begin;

    auto next()
    {
        auto r = cur_;
        ++cur_;
        if(cur_ == end())
            cur_ = begin();
        return *r;
    }

    Ring() = default;

    template<std::ranges::range R>
    explicit Ring(R && range) : 
        begin_{range.begin()}, end_{range.end()}, cur_{begin_}
    { }

private:
    const_iterator_begin begin_{};
    const_iterator_end end_{};
    const_iterator_begin cur_{};
};

template<std::ranges::range R>
auto make_ring(R && range)
{
    return Ring<decltype(range.begin()), decltype(range.end())>{range};
}


#endif