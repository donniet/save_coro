#ifndef __IOSTREAM_INTERFACE_HPP__
#define __IOSTREAM_INTERFACE_HPP__

#include "game.hpp"

template<typename State>
struct iostream_interface : public PlayerInterface<State>
{
    using action_type = PlayerInterface<State>::action_type;

    virtual task<void> display(State const& state) override
    {
        m_os << "board:\n" << state << std::endl;
        co_return;
    }

    virtual task<action_type> 
    select(Ranges<action_type> const& actions) override
    {
        // all players just use is and os
        m_os << "moves available:\n";
        for(auto const& arng : actions)
        {
            m_os << "\t[" << arng.begin() << ", " << arng.end() << ")\n";
        }
        m_os << "choose an action: ";

        action_type act;
        m_is >> act;

        m_os << "\nsending action: " << act << std::endl;
        co_return act;
    }

    iostream_interface(std::istream & is = std::cin, 
        std::ostream& os = std::cout) : m_is{is}, m_os{os}
    { }

private:
    std::istream & m_is;
    std::ostream & m_os;
};


#endif