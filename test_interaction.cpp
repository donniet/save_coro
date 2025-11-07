#include "generator.h"
#include "task.h"
#include "ranges.hpp"
#include "list.hpp"
#include "ring.hpp"
#include "game.hpp"
#include "tictac.hpp"
#include "iostream_interface.hpp"

#include <iostream>
#include <vector>
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <set>


int main(int ac, char * av[])
{
    ThreadedGame<TicTac> game{2};

    iostream_interface<TicTac> player1;
    iostream_interface<TicTac> player2;

    game.add_player(player1);
    game.add_player(player2);

    auto t = turn_based(&game);

    TicTac final = t.get();

    std::cout << "\nwinner: " << final.winner() << std::endl;

    return 0;
}