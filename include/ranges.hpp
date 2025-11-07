#ifndef __RANGES_HPP__
#define __RANGES_HPP__


#include <utility>
#include <set>
#include <concepts>

template<typename T>
concept less_comparable = requires(T a, T b)
{
    a < b;
};

/** 
 * Ranges is a set of non-overlapping ranges of sortable elements T
 */
template<less_comparable T>
class Ranges {
public:
    struct range_type;

    using value_type = range_type;
    using const_iterator = std::set<value_type>::const_iterator;

    struct const_value_iterator;

    void insert(T b, T e) 
    {
        if(e < b) 
            std::swap(b, e);

        if(size() == 0)
        {
            ranges_.emplace(b, e);
            return;
        }

        // assert(ranges_.size() > 0);

        // find the first range which begins with or after b
        auto lo = ranges_.lower_bound(range_type{b, b}); 

        // so long as we aren't first, go back one to ensure we get any
        // intersecting ranges (ranges that end after b)
        if(lo != ranges_.begin())
            --lo;

        // find the first range which begins after e
        auto up = ranges_.upper_bound(range_type{e, e});

        // TODO: do we even need this?
        // // we need to ensure lo->begin() <= up->begin() 
        // if(lo != up && up != ranges_.end())       // up is valid and lo != up
        //     if(lo == ranges_.end() || *up < *lo)  // lo is either invalid or
        //                                           // up->begin() < lo->begin()
        //         std::swap(lo, up);                // then swap them
        //                                           // from now on *lo++ < *up
        //                                           // until lo == up

        // we don't need to check if lo or up are valid because at this point
        // if lo != up then lo cannot be invalid.  
        for(; lo != up;)
        {
            auto c = lo++;
            // do these intersect?
            // b <= c->end
            if(less_or_equal(b, c->end()) && greater_or_equal(e, c->begin()))
            {
                if(c->begin() < b)
                    b = c->begin();

                if(e < c->end())
                    e = c->end();
                    
                ranges_.erase(c);
            }
        }

        // add the range and we know it doesn't intersect any
        ranges_.emplace(b, e);
    }

    const_iterator begin() const { return ranges_.begin(); }
    const_iterator end() const { return ranges_.end(); }

    const_value_iterator values_begin() const
    { return const_value_iterator{ranges_.begin(), ranges_.end()}; }
    std::default_sentinel_t values_end() const 
    { return {}; }

    size_t size() const { return ranges_.size(); }

    bool contains(T const & a) const
    {
        if(size() == 0) 
            return false;

        // find the first range that begins after a
        auto up = ranges_.upper_bound(range_type{a, a});
        // back up one unless we are at the beginning
        if(up != ranges_.begin())
            --up;

        return less_or_equal(up->begin(), a) && a < up->end();
    }

    bool intersects(T const& b, T const& e)
    {
        if(size() == 0)
            return false;
        
        auto up = ranges_.upper_bound(range_type{b, b});
        if(up != ranges_.begin())
            --up;
        
        return b < up->end() && up->begin() < e;
    }
    
protected:
    static constexpr bool equal(T const & a, T const & b)
    { return !(a < b) && !(b < a); }
    static constexpr bool less_or_equal(T const & a, T const & b)
    { return a < b || equal(a, b); }
    static constexpr bool greater(T const & a, T const & b)
    { return !less_or_equal(a, b); }
    static constexpr bool greater_or_equal(T const & a, T const & b)
    { return !less_or_equal(a, b); }

public:
    struct range_type : public std::pair<T,T> {
        T const& begin() const 
        { return std::pair<T,T>::first; }
        T const& end() const 
        { return std::pair<T,T>::second; }

        bool operator<(range_type const & other) const
        { return begin() < other.begin(); }

        range_type(T const& b, T const& e) : 
            std::pair<T,T>{b, e}
        { }
        range_type(std::pair<T,T> const& other) : 
            std::pair<T,T>{other}
        { }
    };

public:
    struct const_value_iterator {
        friend class Ranges;

        const_value_iterator() : i_{}, end_{i_}, cur_{} { }
        const_value_iterator(const_value_iterator &&) = default;
        const_value_iterator(const_value_iterator const &) = default;
        const_value_iterator& operator=(const_value_iterator &&) = default;
        const_value_iterator& operator=(const_value_iterator const&) = default;

        const_value_iterator& operator++()
        { 
            if(i_ == end_)
                return *this;  // already at end

            if(cur_ < i_->end())
                ++cur_;        // increment the value so long as we aren't 
                               // at the end of our current range

            if(!(cur_ < i_->end()))
            {
                ++i_;           // increment our iterator if we pushed past
                if(i_ != end_)
                    cur_ = i_->begin(); // set cur to the begin of the new range
            }

            return *this;
        }

        bool operator==(std::default_sentinel_t) const
        { return i_ == end_; }
        bool operator!=(std::default_sentinel_t) const
        { return i_ != end_; }
        bool operator==(const_value_iterator other) const
        { 
            if(i_ == end_ && other.i_ == other.end_)
                return true;
            if(i_ == end_ || other.i_ == other.end_)
                return false;

            return i_ == other.i_ && cur_ == other.cur_; 
        }
        bool operator!=(const_value_iterator other) const
        { return !(*this == other); }

        T const& operator*() const
        { return cur_; }
        T const* operator->() const
        { return &cur_; }
    private:
        explicit const_value_iterator(const_iterator i, 
                                      const_iterator end) :
            i_{i}, end_{end}, cur_{}
        { 
            if(i_ != end_) 
                cur_ = i_->begin();
        }

        const_iterator i_;
        const_iterator end_;
        T cur_;
    };


private:
    std::set<range_type> ranges_;
};

template<less_comparable T>
class ranges_values_view : 
    public std::ranges::view_interface<ranges_values_view<T>>
{
public:
    ranges_values_view() = default;
    ranges_values_view(Ranges<T> const& ranges) :
        begin_{ranges.values_begin()}
    { }

    auto begin() const { return begin_; }
    std::default_sentinel_t end() const { return {}; }
private:
    typename Ranges<T>::const_value_iterator begin_{};
};



#endif