#ifndef __MINIMAX_HPP__
#define __MINIMAX_HPP__

#include "game.hpp"

#include <map>


// HACK: assumes turn based!
template<typename State>
class MinimaxInterface : public PlayerInterface<State> {
public:
    using action_type = game_traits<State>::action_type;

    virtual task<void> display(State const& state) override
    {
        m_current = state;
        co_return;
    }
    virtual task<action_type> 
    select(Ranges<action_type> const& initial_actions) override
    {
        using std::tie;
        using ranges_iterator = 
            typename Ranges<action_type>::const_value_iterator;

        size_t player_count = 
            PlayerInterface<State>::game_ptr()->players_size();
        // TODO: score for each player
        int score = 0; 
        

        // assumes game is lost if there are no more actions available
        using stack_element = std::tuple<
            State, 
            std::shared_ptr<Ranges<action_type>>, 
            ranges_iterator,
            size_t /* depth */,
            bool /* up */
        >;
            
        std::vector<stack_element> stack;
        stack.emplace_back(m_current, 
            std::make_shared<Ranges<action_type>>(initial_actions), 
            initial_actions.value_begin(), 0, false);

        State state;
        std::shared_ptr<Ranges<action_type>> actions;
        ranges_iterator cur;
        size_t depth;
        bool up;


        size_t player_num = 0; // 0 is the current player

        action_type best_action;
        // HACK: 0 means no score has been found
        int best_score = 0;

        while(!stack.empty())
        {
            // pop the next one off the stack
            tie(state, actions, cur, depth, up) = stack.back();
            stack.pop_back();

            if(up)
            {
                int min_score = 0;

                // if we are headed back up, all the children have been found
                // TODO: this is inefficient to replay these moves
                for(auto i = actions->value_begin(); i != actions->value_end(); 
                    ++i) 
                {
                    auto s = state;
                    s(*i);
                    if(m_scores[s] < min_score)
                        min_score = m_scores[s];
                }

                m_scores[state] = -min_score;
                if(min_score < -best_score)
                {
                    best_score = -min_score;
                    best_action = *cur;
                }
                continue;
            }

            // ensure we catch this state while going up
            stack.emplace_back(state, actions, cur, depth, true);

            // HACK: assumes turn based game
            player_num = depth % player_count;

            // if we aren't at the end, push the next action onto the stack
            if(auto next = cur; ++next != actions->value_end())
                stack.emplace_back(state, actions, next, player_num, false);
            
            // make the move
            state(*cur);

            // do we know how much this one is worth?
            auto i = m_scores.find(state);
            if(i != m_scores.end())
            {
                if(i->second < -best_score)
                {
                    best_score = -i->second;
                    best_action = *cur;
                    continue;
                }
            }

            // we don't know the score, push all possible actions to the 
            // stack
            auto actions1 = 
                std::make_shared<Ranges<action_type>>(state.actions());

            if(actions1->size() == 0)
            {
                // no more actions, so the current player lost
                // that means the player_num made move cur and won the game
                m_scores[state] = -1;
                if(depth == 0 && best_score < 1)
                {
                    best_action = *cur;
                    best_score = 1;
                }
                continue;
            }

            stack.emplace_back(state, actions1, actions1->value_begin(), 
                depth+1, false);
        }

        co_return best_action;
    }

    // DEBUG
    void dump() 
    {
        for(auto i = m_scores.begin(); i != m_scores.end(); i++)
        {
            std::cerr << "\n" << i->second << "\n" << i->first << "\n";
        }
        std::cerr << std::endl;
    }

private:
    State m_current;
    std::map<State, int> m_scores; // always for the current player
};

#endif