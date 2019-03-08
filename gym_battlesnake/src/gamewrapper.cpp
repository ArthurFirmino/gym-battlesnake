#include <vector>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <utility>
#include <functional>
#include <memory>

#include "threadpool.h"
#include "gameinstance.h"
#include "gamerenderer.h"

#define NUM_LAYERS 6
#define LAYER_WIDTH 39
#define LAYER_HEIGHT 39
#define OBS_SIZE NUM_LAYERS * LAYER_WIDTH * LAYER_HEIGHT

struct info {
    unsigned health_;
    unsigned length_;
    unsigned turn_;
    bool alive_;
    bool ate_;
    bool over_;
};

class GameWrapper {

    /* Randomly* orient the board by flipping in x and y */
    unsigned orientation(unsigned game_id, unsigned turn, unsigned player_id) {
        return std::hash<unsigned>{}(game_id) ^ player_id ^ std::hash<unsigned>{}(turn);
    }

    char getaction(unsigned model_i, unsigned env_i, unsigned ori) {
        const char moves[4] = {'u','d','l','r'};
        auto index = acts_[model_i*n_envs_ + env_i];
        char action = moves[index];
        if((ori&1) && (action=='l' || action=='r'))
            action = action=='l'?'r':'l';
        if((ori&2) && (action=='u' || action=='d'))
            action = action=='d'?'u':'d';
        return action;
    }
    
    void writeobs(unsigned model_i, unsigned env_i, unsigned player_id, State gamestate, unsigned ori) {
        /*
            layer0: snake health on heads {0,...,100}
            layer1: snake bodies {0,1}
            layer2: segment numbers {0,...,255}
            layer3: snake length >= player {0,1}
            layer4: food {0,1}
            layer5: gameboard {0,1}
        */
        auto& players = std::get<1>(gamestate);
        Tile head;
        const auto it = players.find(player_id);
        if(it != players.end()) {
            head = it->second.body_.front();
        } else {
            std::abort();
        }
        auto assign = [this, model_i, env_i, head, ori](const Tile& xy, unsigned l, uint8_t val) {
            int x = (int(xy.first) - int(head.first)) * ((ori&1)?-1:1);
            int y = (int(xy.second) - int(head.second)) * ((ori&2)?-1:1);
            x += (LAYER_WIDTH/2);
            y += (LAYER_HEIGHT/2);
            if( x > 0 && x < LAYER_WIDTH && y > 0 && y < LAYER_HEIGHT)
                obss_[ model_i*(n_envs_*OBS_SIZE) + env_i*OBS_SIZE + x*(LAYER_HEIGHT*NUM_LAYERS) + y*NUM_LAYERS + l] = val;
        };

        unsigned playersize = it->second.body_.size();
        for(const auto& p: players) {
            if(!p.second.alive_) 
                continue;
            assign(p.second.body_.front(), 0, p.second.health_);
            unsigned i = 0;
            for(auto cit = p.second.body_.crbegin(); cit != p.second.body_.crend(); ++cit) {
                assign(*cit, 1, 1);
                assign(*cit, 2, std::min(++i, static_cast<unsigned>(255)));
            }
            if(p.second.id_ != player_id)
                assign(p.second.body_.front(), 3, p.second.body_.size() >= playersize ? 1 : 0 );
        }

        auto& food = std::get<2>(gamestate);
        for(const auto& xy: food) assign(xy, 4, 1);

        for(int x=0; x < static_cast<int>(std::get<3>(gamestate)); ++x) {
            for(int y=0; y < static_cast<int>(std::get<4>(gamestate)); ++y) {
                assign({x,y}, 5, 1);
            }
        }
    }

public:

    GameWrapper(unsigned n_threads, unsigned n_envs, unsigned n_models) : 
        n_threads_(n_threads), n_envs_(n_envs), n_models_(n_models), threadpool_(n_threads), gr_(nullptr) {
        // 1. Create envs
        envs_.resize(n_envs, nullptr);
        // 2. Allocate obs and act arrays
        obss_.resize(n_models * n_envs * OBS_SIZE);
        acts_.resize(n_models * n_envs);
        info_.resize(n_envs);
        // 3. Reset envs
        reset();
    }

    ~GameWrapper() = default;

    void reset() {
        // Clear obs arrays
        memset(&obss_[0], 0, obss_.size() * sizeof obss_[0]);
        // Reset all envs
        for(unsigned ii{0}; ii < n_envs_; ++ii) {
            threadpool_.schedule([this,ii](){
                auto& gi = envs_[ii];
                gi.reset();
                // Create new game instance
                unsigned bwidth = (std::rand() % (1+19-7)) + 7;
                unsigned bheight = (std::rand() % (1+19-7)) + 7;
                unsigned nfood = (std::rand() % 8) + 1;
                gi = std::make_shared<GameInstance>(bwidth, bheight, n_models_, nfood);
                // Write states into observation arrays
                auto ids = gi->getplayerids();
                auto state = gi->getstate();
                for(unsigned m{0}; m < n_models_; ++m) {
                    writeobs(m, ii, ids[m], state, orientation(gi->gameid(), gi->turn(), ids[m]));
                }
                info_[ii].health_ = 100;
                info_[ii].length_ = PLAYER_STARTING_LENGTH;
                info_[ii].turn_ = 0;
                info_[ii].alive_ = true;
                info_[ii].ate_ = false;
                info_[ii].over_ = false;
            });
        }
        threadpool_.wait();
    }

    void step() {
        // Clear obs arrays
        memset(&obss_[0], 0, obss_.size() * sizeof obss_[0]);
        // Step all envs
        for(unsigned ii{0}; ii < n_envs_; ++ii) {
            threadpool_.schedule([this,ii](){

                auto& gi = envs_[ii];
                // Read actions into gameinstance
                auto ids = gi->getplayerids();
                for(unsigned m{0}; m < n_models_; ++m) {
                    gi->setplayermove(ids[m], getaction(m, ii, orientation(gi->gameid(), gi->turn(), ids[m]) ) );
                }
    
                // Get player length before step
                auto player_id = ids[0];
                auto it = std::get<1>(gi->getstate()).find(player_id);

                // Step game
                gi->step();

                // Figure out if done
                it = std::get<1>(gi->getstate()).find(player_id);
                bool done = !(it->second.alive_) || gi->over();

                // Write info
                info_[ii].health_ = it->second.health_;
                info_[ii].length_ = it->second.body_.size();
                info_[ii].turn_ = gi->turn();
                info_[ii].alive_ = it->second.alive_;
                info_[ii].ate_ = it->second.health_ == 100 && gi->turn() > 0;
                info_[ii].over_ = done;
                
                // Reset game if over
                if(done) {
                    gi.reset();
                    // Create new game instance
                    unsigned bwidth = (std::rand() % (1+19-7)) + 7;
                    unsigned bheight = (std::rand() % (1+19-7)) + 7;
                    unsigned nfood = (std::rand() % 8) + 1;
                    gi = std::make_shared<GameInstance>(bwidth, bheight, n_models_, nfood);
                    ids = gi->getplayerids();
                }
                // Write states into observation arrays
                for(unsigned m{0}; m < n_models_; ++m) {
                    writeobs(m, ii, ids[m], gi->getstate(), orientation(gi->gameid(), gi->turn(), ids[m]));
                }
            });
        }
        threadpool_.wait();
    }
    
    void render() {
        if(!gr_) {
            gr_.reset(new GameRenderer(800,600));
            gr_->init();
        }
        gr_->attach(envs_[0]);
        gr_->render();
    }

    unsigned n_threads_, n_envs_, n_models_;
    ThreadPool threadpool_;
    std::unique_ptr<GameRenderer> gr_;
    std::vector<std::shared_ptr<GameInstance>> envs_;
    std::vector<uint8_t> obss_;
    std::vector<uint8_t> acts_;
    std::vector<info> info_;
};

extern "C" {
    GameWrapper* env_new(unsigned n_threads, unsigned n_envs, unsigned n_models) {
        return new GameWrapper(n_threads, n_envs, n_models);
    }
    void env_delete(GameWrapper *p) {
        delete p;
    }
    void env_reset(GameWrapper *p) {
        p->reset();
    }
    void env_step(GameWrapper *p) {
        p->step();
    }
    void env_render(GameWrapper *p) {
        p->render();
    }
    uint8_t* env_getobspointer(GameWrapper *p, unsigned model_i) {
        return &p->obss_[model_i * (p->n_envs_ * OBS_SIZE)];
    }
    uint8_t* env_getactpointer(GameWrapper *p, unsigned model_i) {
        return &p->acts_[model_i * p->n_envs_];
    }
    info* env_getinfopointer(GameWrapper *p) {
        return &p->info_[0];
    }
}