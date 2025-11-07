#ifndef __GAME_HPP__
#define __GAME_HPP__

#include "ranges.hpp"
#include "task.hpp"

#include <stdexcept>
#include <thread>
#include <concepts>
#include <mutex>
#include <condition_variable>
#include <list>
#include <functional>

// DEBUG
#include <iostream>

using game_error = std::logic_error;


template<typename T>
struct game_traits
{
    using action_type = typename T::action_type;
    static Ranges<action_type> actions(T const& game)
    { return game.actions(); }
};

template<typename State>
class PlayerInterface { 
public:
    using action_type = game_traits<State>::action_type;

    size_t id() const { return m_id; }
    operator size_t() const { return m_id; }

    PlayerInterface() : m_id(0) { }
    explicit PlayerInterface(size_t id) : m_id(id) { }

    virtual task<void> display(State const&) = 0;
    virtual task<action_type> select(Ranges<action_type> const&) = 0;

private:
    size_t m_id;
};

template<typename State>
struct GameInterface {
    using action_type = game_traits<State>::action_type;
    using player_type = PlayerInterface<State>;

    virtual task<List<player_type*>> players() 
    { co_return m_players; }

    virtual task<void> display(State const& state) = 0;

    void add_player(player_type & player)
    { m_players.push_back(&player); }

    auto players_begin() const
    { return m_players.begin(); }

    auto players_end() const
    { return m_players.end(); }

    auto players_view() const
    { return std::ranges::views::all(m_players); }

    GameInterface() : m_players{} { }
private:
    std::vector<player_type*> m_players;
};

template<typename State>
struct ThreadedGame : public GameInterface<State> 
{
    using work_type = std::function<void()>;
    using work_list_type = std::list<work_type>;
    using player_type = GameInterface<State>::player_type;

    virtual task<void> display(State const& state) override
    {
        std::mutex local_mutex;
        std::condition_variable_any local_cond;

        std::unique_lock local_lock(local_mutex);
        std::unique_lock lock(m_mutex);

        // use the workers to send the state to the players as quickly as 
        // possible
        size_t player_count = 0;
        for(auto * player : GameInterface<State>::players_view())
        {
            ++player_count;

            add_work([&]() {
                auto task = player->display(state);
                task.get(); // this is a coroutine

                local_lock.lock();
                --player_count;
                local_lock.unlock();
                local_cond.notify_one();
            }); 
        }

        lock.unlock();
        m_cond.notify_all();

        local_cond.wait(local_lock, [&]() { return player_count == 0; });
        co_return;
    }

    virtual void start()
    { }

    virtual void stop()
    { m_stopper.request_stop(); }


    ThreadedGame(size_t thread_count) : 
        GameInterface<State>{}, m_threads(thread_count)
    { 
        for(auto *& thread : m_threads)
            startup(thread);
            
        start();
    }

    ~ThreadedGame() 
    { 
        stop();
        for(auto *& thread : m_threads)
            shutdown(thread);
        m_threads.clear();
    }


protected:
    void startup(std::jthread *& thread)
    { 
        // HACK: assumes a vector container
        size_t id = std::distance(&m_threads[0], &thread);

        thread = new std::jthread(&ThreadedGame::worker, this,
            id, m_stopper.get_token()); 
    }

    void shutdown(std::jthread *& thread)
    { 
        delete thread; 
        thread = nullptr;
    }

    void worker(size_t id, std::stop_token stoken)
    {
        for(;;)
        {
            work_list_type::iterator work_ptr;

            std::unique_lock lock(m_mutex);
            bool has_work = m_cond.wait(lock, stoken, [this, id]{ 
                return work_available_for(id); });
            
            if(stoken.stop_requested())
                break;

            if(!has_work)
                continue;

            work_ptr = begin_work_for(id);
            lock.unlock(); // unlock for work to begin;

            (*work_ptr)();

            lock.lock();  // relock to mark work as complete
            complete_work(work_ptr);
            lock.unlock();
            m_cond.notify_one();
        }
        std::cerr << "shutting down thread" << std::endl;
    }
protected:
    /**
     * Worker methods
     * 
     * These must be executed under a locked m_mutex;
     */
    bool work_available_for(size_t id)
    { return m_work.size() > id; }

    work_list_type::iterator begin_work_for(size_t id)
    {  
        auto e = m_work.begin(),
             b = e++;

        m_work_began.splice(m_work_began.end(), m_work, b, e);
        return b;
    }

    void complete_work(work_list_type::iterator work_ptr)
    { m_work_began.erase(work_ptr); }
 
    void add_work(work_type && work)
    { m_work.emplace_back(std::move(work)); }

private:
    work_list_type m_work;
    work_list_type m_work_began;
    

    std::vector<std::jthread *> m_threads;
    std::stop_source m_stopper;
    std::mutex m_mutex;
    std::condition_variable_any m_cond;
};

template<typename State>
task<State> turn_based(GameInterface<State> * game)
{
    auto players = co_await game->players();
    auto player_ring = make_ring(players);

    State state{}; // creates the initial state of the game   

    // go until the game is complete
    while(state)
    {
        // send the board out to the player interface
        co_await game->display(state);

        // what actions are available to the player?
        auto actions = state.actions();
        // allow the next player to select an action
        auto selected = co_await player_ring.next()->select(actions);
        // update the board with the action
        if(!state(selected))
            throw game_error("action rejected by state");
        // yield the state after this move
    }

    co_await game->display(state);

    co_return state;
}


#endif