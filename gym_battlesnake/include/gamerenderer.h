#ifndef GAME_RENDERER_H
#define GAME_RENDERER_H

#include <memory>
#include <functional>
#include <SFML/Graphics.hpp>
#include "gameinstance.h"

class GameRenderer {
    public:
        GameRenderer(unsigned res_w, unsigned res_h) : res_w_(res_w), res_h_(res_h), gi_(nullptr), window_(nullptr) {
        }
        
        GameRenderer(const GameRenderer&) = delete;
        GameRenderer(GameRenderer&&) = delete;
        
        ~GameRenderer() = default;

        void init() {
            window_.reset( new sf::RenderWindow(sf::VideoMode(res_w_, res_h_), "BattlesnakeEnv", sf::Style::Resize | sf::Style::Close));
            window_->setFramerateLimit(30);
        }

        void attach(std::shared_ptr<GameInstance> gi) {
            gi_ = gi;
        }

        void render() {

            if(!window_->isOpen()) return;
            sf::Event event;
            while(window_->pollEvent(event)) {
                if(event.type == sf::Event::Closed) {
                    window_->close();
                    return;
                }
            }

            Parameters param = gi_->getparameters();
            unsigned board_w = std::get<0>(param);
            unsigned board_h = std::get<1>(param);
            float board_aspect_ratio = board_w / (float) board_h;
            sf::RectangleShape background(sf::Vector2f(board_w, board_h));
            background.setFillColor(sf::Color(127,127,127));
            auto drawTile = [this](unsigned x, unsigned y, sf::Color color) {
                sf::RectangleShape tile(sf::Vector2f(1.0f, 1.0f));
                tile.scale(sf::Vector2f(0.8f, 0.8f));
                tile.setOrigin(-0.1f, -0.1f);
                tile.setPosition(float(x), float(y));
                tile.setFillColor(color);
                this->window_->draw(tile);
            };
            auto res = window_->getSize();
            res_w_ = res.x;
            res_h_ = res.y;
            float window_aspect_ratio = res_w_ / (float) res_h_;
            if(board_aspect_ratio > window_aspect_ratio) {
                sf::Vector2f v(board_w, board_w / window_aspect_ratio);
                window_->setView(sf::View(0.5f*v, v));
            } else {
                sf::Vector2f v(board_h * window_aspect_ratio, board_h);
                window_->setView(sf::View(0.5f*v, v));
            }

            // Draw background
            window_->clear(sf::Color(31,31,31));
            window_->draw(background);
            // Draw board tiles
            for(unsigned i {0}; i < board_w; ++i) {
                for(unsigned j {0}; j < board_h; ++j) {
                    // Determine color
                    sf::Color color;
                    unsigned tile_id = gi_->tileid(i,j);
                    if(tile_id == 0) {
                        color = sf::Color(159,159,159);
                    } else if(tile_id == FOOD_ID) {
                        color = sf::Color::Green;
                    } else {
                        uint32_t cid = static_cast<uint32_t>(tile_id);
                        cid = cid ^ 0xA2343434;
                        color = sf::Color(std::hash<uint32_t>{}(cid));
                    }
                    // Draw tile
                    drawTile(i,j,color);
                }
            }
            window_->display();
        }

    private:
        unsigned res_w_, res_h_;
        std::shared_ptr<GameInstance> gi_;
        std::unique_ptr<sf::RenderWindow> window_;
};

#endif