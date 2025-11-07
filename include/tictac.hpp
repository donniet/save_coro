#ifndef __TICTAC_HPP__
#define __TICTAC_HPP__

#include "ranges.hpp"
#include "game.hpp"

#include <iostream>
#include <array>

class TicTac { 
public:
    enum Mark { 
        Blank = 0,
        X = 1,
        O = 2,
        Cats = 3,
    };

    enum Move {
        TopLeft    = 8, TopCenter    = 5, TopRight    = 2,
        CenterLeft = 7, Center       = 4, CenterRight = 1,
        BottomLeft = 6, BottomCenter = 3, BottomRight = 0,
        TotalMoves = 9,
    };

    Mark turn() const
    { 
        int moves = 0;
        for(auto const& m : m_board)
            moves += (m == Blank ? 0 : 1);

        if(moves == 9)
            return Blank;
        
        return moves % 2 == 1 ? O : X;
    }
    Mark winner() const
    {
        static constexpr Move s_winners[][3] = {
            {TopLeft, TopCenter, TopRight},
            {CenterLeft, Center, CenterRight},
            {BottomLeft, BottomCenter, BottomRight},
            
            {TopLeft, CenterLeft, BottomLeft},
            {TopCenter, Center, BottomCenter},
            {TopRight, CenterRight, BottomRight},

            {TopLeft, Center, BottomRight},
            {TopRight, Center, BottomLeft},
        };

        // is there a winning triplet of non-blanks?
        for(auto& tri : s_winners)
            if(m_board[tri[0]] != Blank && 
               m_board[tri[0]] == m_board[tri[1]] && 
               m_board[tri[1]] == m_board[tri[2]])
                return m_board[tri[0]]; // return it
        
        if(turn() == Blank)
            return Cats;

        return Blank; 
    }
    Mark& at(Move m)             { return m_board[(unsigned)m]; }
    Mark const& at(Move m) const { return m_board[(unsigned)m]; }

    Mark& at(int r, int c)             { return m_board[3 * r + c]; }
    Mark const& at(int r, int c) const { return m_board[3 * r + c]; }

    // standard game interface
    auto operator()(Move m) { return play(m); }
    operator bool() const   { return !done(); }
    // used for storing state in sorted containers
    bool operator==(TicTac const& other) const 
    { return m_board == other.m_board; }
    bool operator<(TicTac const& other) const
    { return m_board < other.m_board; }

    bool done() const
    { return winner() != Blank; }
    bool play(Move m)
    {
        if(at(m) != Blank)
            return false;
    
        at(m) = turn();
        return true;
    }

    using action_type = Move;

    Ranges<action_type> actions() const
    { 
        Ranges<action_type> ret;

        if(winner() != Blank)
            return ret;

        int range_start = -1; // not started
        for(int m = 0; m < 9; m++)
        {
            if(at((Move)m) != Blank)
            {
                if(range_start >= 0)
                    ret.insert((Move)range_start, (Move)m);
                range_start = -1;
                continue;
            }
            
            if(range_start < 0)
                range_start = m;
        }
        if(range_start >= 0)
            ret.insert((Move)range_start, TotalMoves);

        return ret;
    }

    TicTac() : m_board{Blank} { }

    std::array<Mark, TotalMoves> m_board;
};

TicTac::Move & operator++(TicTac::Move & move) 
{ 
    move = static_cast<TicTac::Move>((int)move + 1);
    return move; 
}

std::ostream& operator<<(std::ostream& os, TicTac::action_type const& act)
{
    int m = (int)act;
    os << m / 3 << " " << m % 3;
    return os;
}

std::istream& operator>>(std::istream& is, TicTac::action_type& act)
{
    int r, c;
    is >> r >> c;
    act = (TicTac::action_type)(r * 3 + c);
    return is;
}

std::ostream& operator<<(std::ostream& os, TicTac::Mark mark)
{
    char c;

    switch(mark) {
    case TicTac::X: c = 'X'; break;
    case TicTac::O: c = 'O'; break;
    case TicTac::Cats: c = 'C'; break;
    case TicTac::Blank: 
    default: 
        c = ' ';
        break;
    }

    return os << c; 
}

std::ostream& operator<<(std::ostream& os, TicTac const& board)
{
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            os << board.at((TicTac::Move)(3 * i + j));
            if(j < 2) 
                os << "|";
        }

        if(i < 2)
            os << "\n-----";

        os << "\n";
    }
    os << "turn: " << board.turn() << "\n";

    return os;
}




#endif