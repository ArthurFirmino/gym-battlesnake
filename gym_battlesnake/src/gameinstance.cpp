#include <random>
#include <cstring>

#include "gameinstance.h"

std::random_device rd;
std::mt19937 gen(rd());

unsigned next_game_id = 1000000;

GameInstance::GameInstance(unsigned board_width, unsigned board_length, unsigned num_players, unsigned num_food) :
	board_width_(board_width), board_length_(board_length), num_players_(num_players), num_food_(num_food), food_(2*num_food) {

	// Set parameters
	game_id_ = next_game_id++;
	over_ = false;
	turn_ = 0;

	// Create board
	board_.resize(board_width_*board_length_, 0);
	players_.reserve(2*num_players_);
	std::uniform_int_distribution<> X(0,board_width_-1);
	std::uniform_int_distribution<> Y(0,board_length_-1);

	// Place players
	std::uniform_int_distribution<> ID(1000000,9999999);
	for(unsigned i {0}; i < num_players_; ++i) {
		unsigned id;
		do {
			id = static_cast<unsigned>(ID(gen));
		} while(players_.find(id) != players_.end());
		auto pit = players_.emplace(std::make_pair(id, id));
		unsigned x, y;
		do {
			x = static_cast<unsigned>(X(gen));
			y = static_cast<unsigned>(Y(gen));
		} while(at(x,y) != 0);
		at(x,y) = id;
		for(int j{0}; j < PLAYER_STARTING_LENGTH; ++j) {
			pit.first->second.body_.push_back({x,y});
		}
	}

	// Place food
	for(unsigned i {0}; i < num_food_; ++i) {
		unsigned x, y;
		do {
			x = static_cast<unsigned>(X(gen));
			y = static_cast<unsigned>(Y(gen));
		} while(at(x,y) != 0);
		at(x,y) = FOOD_ID;
		food_.insert({x,y});
	}
}

void GameInstance::step() {

	++turn_;
	std::unordered_set<unsigned> players_to_kill;

	// Move players, check for out of bounds, self collisions, and food
	for(auto& p : players_) {
		// Skip dead players
		if(!p.second.alive_) continue;
		// Subtract health
		--p.second.health_;
		// Next head location
		Tile curr_head = p.second.body_.front();
		char move = p.second.move_;
		Tile next_head = curr_head;
		switch(move) {
			case 'u': {
				--next_head.second;
				break;
			} case 'd' : {
				++next_head.second;
				break;
			} case 'l' : {
				--next_head.first;
				break;
			} case 'r' : {
				++next_head.first;
				break;
			}
		}
		// Check out of bounds, then check food
		if(next_head.first < 0 || next_head.first >= board_width_ || next_head.second < 0 || next_head.second >= board_length_) {
			players_to_kill.insert(p.second.id_);
			p.second.body_.pop_back();
		} else if(at(next_head) == FOOD_ID) {
			p.second.health_ = 100;
			p.second.body_.push_front(next_head);
			food_.erase(next_head);
		} else {
			p.second.body_.pop_back();
			p.second.body_.push_front(next_head);
		}
		// Starvation
		if(p.second.health_ == 0) {
			players_to_kill.insert(p.second.id_);
		} 
	}

	// Reset board, add player bodies, map heads
	memset(&board_[0], 0, board_.size() * sizeof board_[0]);
	std::unordered_multimap<Tile, unsigned> heads;
	for(const auto& p : players_ ) {
		if(!p.second.alive_) continue;

		auto it = p.second.body_.begin();
		heads.insert({*it, p.second.id_});
		++it;
		for(; it != p.second.body_.end(); ++it) {
			at(*it) = p.second.id_;
		}
	}

	// Check head on head collisions
	for(auto& p : players_) {
		if(!p.second.alive_) continue;

		unsigned length = p.second.body_.size();
		auto range = heads.equal_range(*(p.second.body_.begin()));
		unsigned opplength = 0;
		for(auto it = range.first; it != range.second; ++it) {
			if(it->second != p.second.id_) {
				auto q = players_.find(it->second);
				auto l = q->second.body_.size();
				opplength = l > opplength ? l : opplength;
			}
		}
		if(opplength >= length)
			players_to_kill.insert(p.second.id_);
	}	
	
	// Check for collisions with bodies
	for(auto& p : players_) {
		if(!p.second.alive_) continue;

		auto head = p.second.body_.front();
		if(at(head) >= 1000000) {
			players_to_kill.insert(p.second.id_);
		}
	}

	// Kill players
	for(auto& id : players_to_kill) {
		players_.find(id)->second.alive_ = false;
	}

	// Add new food
	std::uniform_int_distribution<> X(0,board_width_-1);
	std::uniform_int_distribution<> Y(0,board_length_-1);
	unsigned loopiter = 0;
	while(food_.size() < num_food_) {
		unsigned x, y;
		do {
			x = static_cast<unsigned>(X(gen));
			y = static_cast<unsigned>(Y(gen));
			if(++loopiter >= 1000u) 
				break;
		} while(at(x,y) != 0);
		if(loopiter >= 1000u) 
			break;
		at(x,y) = FOOD_ID;
		food_.insert({x,y});
	}

	// Reset board, set players, and food
	memset(&board_[0], 0, board_.size() * sizeof board_[0]);
	int players_alive {0};
	for(const auto& p : players_) {
		if(!p.second.alive_) continue;
		++players_alive;
		for(const auto& b : p.second.body_) at(b) = p.second.id_;
	}

	over_ = (players_alive <= 1 && num_players_ > 1) || (players_alive == 0 && num_players_ == 1);
	for(const auto& f : food_) at(f) = FOOD_ID;
}