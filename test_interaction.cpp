#include "generator.hpp"
#include "task.hpp"
#include "ranges.hpp"
#include "list.hpp"
#include "ring.hpp"
#include "game.hpp"
#include "tictac.hpp"
#include "iostream_interface.hpp"
#include "minimax.hpp"

#include <iostream>
#include <vector>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <set>


int main(int ac, char * av[])
{
    GameInterface<TicTac> game;

    iostream_interface<TicTac> player1;
    MinimaxInterface<TicTac> computer;

    game.add_player(player1);
    game.add_player(computer);

    auto t = turn_based(&game);

    TicTac final = t.get();

    std::cout << "\nwinner: " << final.winner() << std::endl;

    // DEBUG
    // computer.dump();

    return 0;
}