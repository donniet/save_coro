import atomic from "threading";
import Pile from "games";

/**
 * Helper Functions 
 */

// transfers amount between from and to Piles atomicly
function atomic_transfer(from, amount, to) {
  atomic(() => {
    const escrow = from.take(amount);
    to.give(escrow);
  });
}

// helper to calculate the minimum bet of all the players so far
function minimum_player_bet(table) {
  return table.bets.reduce(min, Infinity);
};

// removes the player from this round and deletes their bet
// TODO: we probably need another way to track player bets so we can 
//       report at the end how much each player bet in total this round
function fold(player, round) {
  round.remove(player);
  delete table.bets[player];
};

// helper to take the required amount of chips from a player or fold if they
// do not have enough
function take_xor_fold(player, amount, round) {
  // does the player have enough to cover the amount?
  if(player.chips.total() < amount)
    return fold(player, round);

  // ensure that there is no way to interrupt the transfer of chips
  atomic(() => {
    player.chips.take(amount);
    table.chips.give(amount);
  });

  // update our max bet
  let player_bet = (table.player_bets[player] += amount);
  table.table_bet = max(player_bet, table_bet);
  return amount;
};

// helper that deals the hand to each player
function deal(deck, players) {
  let cards = deck.shuffle();
  for(let player of players)
    player.hand.give(cards.take(2));

  return cards;
};

// the flop, turn and river
function flop(cards) { 
  atomic_transfer(cards, 3, table.cards);
}
function turn(cards) { 
  atomic_transfer(cards, 1, table.cards);
}
function river(cards) {
  atomic_transfer(cards, 1, table.cards);
}

// helper that takes bets from remaining players
async function bet(round, table, first_to_bet) {
  let once_around = false; 

  let all_in_tracker = [];

  while(true) {
    // get the next player from the round
    const player = round.next();
    const player_bet = table.bets[player]; // this player's current bet

    // is the player all in?
    if(player.chips.total() == 0)
      continue; // can't bet anymore

    // ask the player for a bet
    const amount = await player.bet_minimum_of(table.table_bet - player_bet);

    // call or raise
    if(player_bet + amount >= table_bet) {
      let is_all_in = false;
      if(amount == player.chips.total()) {
        is_all_in = true;
        await table.announce(`all_in: "${player}"`);
      }

      // divide up the player's bet among the all_in pots and table, in order
      for(let all_in of all_in_tracker) {
        // do we owe chips to this all_in pot?
        // NOTE: this will never be true for a player's own all_in pot
        if(player_bet < all_in.total_bet) {
          let amount_due = min(amount, all_in.total_bet - player_bet);
          // transfer what is due to this all_in pot
          atomic_transfer(player.chips, amount_due, all_in.chips);

          if(amount -= amount_due == 0)
            break;
        }
      }

      if(is_all_in) {
        // if we are all in, create a tracker, and transfer the table pot and 
        // any remaining chips from the bet to their pot
        all_in_tracker.push({
          player: player, 
          total_bet: table.table_bet + amount,
          chips: new Pile(0),
        });
        // move the table chips to the all_in pot for this player
        atomic_transfer(table.chips, table.chips.total(), 
                        all_in_tracker.at(-1).chips);
        // move remaining player chips into the player's all in pot
        atomic_transfer(player.chips, amount, 
                        all_in_tracker.at(-1).chips);
      }
      else {
        // if we aren't all in, put any remaining chips into the table's pot
        atomic_transfer(player.chips, amount, table.chips);
      }
    }
    // fold
    else 
      fold(player, round);

    // move the chips that were bet in an atomic fashion
    atomic_transfer(player.chips, amount, table.chips);
    table.bets[player] += amount;

    // adjust the table bet
    table.table_bet = player_bet + amount;

    // check if we've gone once around
    if(!once_around && round.peek() === first_to_bet)
      once_around = true;

    // update the view for all the players
    await table.update_view();
  }
};

// this has all the poker logic in it and returns the index of the winning cards
// from the player_cards array
// TODO: handle ties!
const determine_winner = function(table_cards, player_cards) {
  /* ... */
};

// clean_up should get the table setup before a round starts
const clean_up = function(table) { 
  /* ... */ 
};

// TODO: handle ties!
async function divide_winnings(table, round) {
  // give away all our chips
  // TODO: write the all_in_pots_are_not_empty function

next_player_to_win:
  while(table.chips.total() > 0 && all_in_pots_are_not_empty()) {
    let winner = 
      determine_winner(table.cards, round.map(player => player.cards));
    let winning_player = round[winner];

    // go through the all_in pots collecting chips until we get to ours
    while(table.all_in.length > 0) {
      // shift out the first one
      let all_in = table.all_in.shift();

      atomic_transfer(all_in.chips, all_in.chips.total(), winning_player.chips);

      if(all_in.player === winning_player) {
        // this player got their winnings, removing them
        round.remove(winning_player);
        continue next_player_to_win;
      }
    }
    
    // give the dude what he's due                           
    atomic_transfer(table.chips, table.chips.total(), round[winner].chips);
    clean_up(table);
  }
}


/**
 * holdem_rules implements basic rules of little/big blind, no-limit texas
 * holdem.
 * 
 * deck: a cards factory which returns a randomized pile of cards via .shuffle()
 * players: an array of players, each of which has:
 *  .chips: a pile of chips
 *  .hand: a pile of cards only visible to this player
 *  .bet_minimum_of: an async function which takes a bet from the player
 * table: the UI interface for all players
 *  .chips: a pile of chips
 *  .cards: a pile of cards visible to all players
 *  .table_bet: the current bet each player must meet to call
 *  .bets: a map of all the players current bets
 *  .seat_players: initializes internals for given players
 */
async function holdem_rules(deck, players, table, big = 2, little = 1) {
  const phases = ['BLINDS', 'DEAL', 'FLOP', 'TURN', 'RIVER'];

  var round,         // variable to store the turn order
      button_on,     // button is on this player
      first_to_bet;  // this player is the first to bet

/**
 * Main Game Loop
 */
round_begin:
  for(let round_count = 0; ; round_count++) {
    // we'll do the winner determination at the top of the loop to allow for 
    // named continues within the body of the loop but we'll skip this on the 
    // first round since there can be no winner yet
    if(round_count > 0)
      await divide_winnings(table, round);

    clean_up(table);

    // setup the table and wait for players to be ready
    round = await table.seat_players(players, round_count);

    // loop for each phase of the game
    for(let phase of phase_order) {
      await table.announce(`phase: "${phase}"`);

      // variable to hold the shuffled cards
      var cards;

      switch(phase) {
      case 'BLINDS':
        // if they can't afford the littl blind they fold
        while(!take_xor_fold(round.next(), little)) 
          if(round.size() == 1)
            continue round_begin;

        button_on = round.peek_prev();

        // same for the big blind
        while(!take_xor_fold(round.next(), big))
          if(round.size() == 1)
            continue round_begin;

        first_to_bet = round.peek_next();
        break;
      case 'DEAL':
        // shuffle up and deal
        cards = deal(deck, round);
        break;
      case 'FLOP':
        flop(cards);
        break;
      case 'TURN':
        turn(cards);
        break;
      case 'RIVER':
        river(cards);
        break;
      } 

      // update the player view and then take bets
      await table.update_view(); 
      await bets(round);

      // if we are down to 1 player, this round is over
      // continue at round_begin
      if(round.size() == 1)
        continue round_begin;
    }
  }
  
}