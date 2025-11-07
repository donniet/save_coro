#include "ranges.hpp"

#include <iostream>

/**
 * test non-intersecting ranges library
 */
int main(int ac, char * av[])
{
    Ranges<int> int_ranges;
    int_ranges.insert(0, 10);
    int_ranges.insert(20, 30);
    int_ranges.insert(25, 40);

    if(int_ranges.contains(45))
        throw std::logic_error("error: ranges should not contain 45");

    if(!int_ranges.contains(0))
        throw std::logic_error("error: should contain 0");

    if(int_ranges.contains(10))
        throw std::logic_error("error: should not contain 10 as it is the end "
                               "of a ranges");

    if(!int_ranges.contains(5))
        throw std::logic_error("error: should contain 5");

    if(int_ranges.contains(15))
        throw std::logic_error("error: should not contain 15");

    if(!int_ranges.contains(20))
        throw std::logic_error("error should contain 20");
        
    if(!int_ranges.contains(30))
        throw std::logic_error("error: should contain 30");

    if(int_ranges.contains(40))
        throw std::logic_error("error: should not contain 30");

    auto i = int_ranges.begin();
    if(i->begin() != 0 || i->end() != 10)
        throw std::logic_error("error: first range should be [0,10)");
    ++i;
    if(i->begin() != 20 || i->end() != 40)
        throw std::logic_error("error: second range should be [20,40)");
    ++i;
    if(i != int_ranges.end())
        throw std::logic_error("error: should only be 2 ranges after "
                               "intersections were removed");

    // print out all the values
    ranges_values_view values{int_ranges};
    for(int x : values)
        std::cout << x << ", ";
    std::cout << std::endl;


    return 0;
}