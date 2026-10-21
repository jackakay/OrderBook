#include "order_book.h"
#include <cstdlib>
#include <algorithm>

void OrderBook::addLimitOrder(uint64_t id, double price, int quantity, bool is_buy) {
    Order order{id, price, quantity, is_buy};

    if (is_buy) {
        // Simple matching against asks (selling queue)
        while (quantity > 0 && !asks_.empty() && asks_.begin()->first <= price) {
            auto& best_ask_level = asks_.begin()->second;
            auto& top_order = best_ask_level.front();

            if (top_order.quantity <= quantity) {
                quantity -= top_order.quantity;
                best_ask_level.pop_front();
                if (best_ask_level.empty()) {
                    asks_.erase(asks_.begin());
                }
            } else {
                top_order.quantity -= quantity;
                quantity = 0;
            }
        }
        // If unfulfilled, add remaining quantity to bids
        if (quantity > 0) {
            order.quantity = quantity;
            bids_[price].push_back(order);
        }
    } else {
        // Simple matching against bids (buying queue)
        while (quantity > 0 && !bids_.empty() && bids_.begin()->first >= price) {
            auto& best_bid_level = bids_.begin()->second;
            auto& top_order = best_bid_level.front();

            if (top_order.quantity <= quantity) {
                quantity -= top_order.quantity;
                best_bid_level.pop_front();
                if (best_bid_level.empty()) {
                    bids_.erase(bids_.begin());
                }
            } else {
                top_order.quantity -= quantity;
                quantity = 0;
            }
        }
        // If unfulfilled, add remaining quantity to asks
        if (quantity > 0) {
            order.quantity = quantity;
            asks_[price].push_back(order);
        }
    }
}
void OrderBook::addMarketOrder(uint64_t id, int quantity, bool is_buy) {
    if (is_buy) {
        // Match against the best ask
        while (quantity > 0 && !asks_.empty()) {
            auto& best_ask_level = asks_.begin()->second;
            auto& top_order = best_ask_level.front();

            if (top_order.quantity <= quantity) {
                quantity -= top_order.quantity;
                best_ask_level.pop_front();
                if (best_ask_level.empty()) {
                    asks_.erase(asks_.begin());
                }
            } else {
                top_order.quantity -= quantity;
                quantity = 0;
            }
        }
    } else {
        // Match against the best bid
        while (quantity > 0 && !bids_.empty()) {
            auto& best_bid_level = bids_.begin()->second;
            auto& top_order = best_bid_level.front();

            if (top_order.quantity <= quantity) {
                quantity -= top_order.quantity;
                best_bid_level.pop_front();
                if (best_bid_level.empty()) {
                    bids_.erase(bids_.begin());
                }
            } else {
                top_order.quantity -= quantity;
                quantity = 0;
            }
        }
    }
}

void OrderBook::cancelOrder(uint64_t id) {
    // Basic search and removal helper for simulation
    for (auto it = bids_.begin(); it != bids_.end(); ++it) {
        auto& list = it->second;
        list.remove_if([id](const Order& o) { return o.id == id; });
        if (list.empty()) {
            bids_.erase(it);
            break;
        }
    }
    for (auto it = asks_.begin(); it != asks_.end(); ++it) {
        auto& list = it->second;
        list.remove_if([id](const Order& o) { return o.id == id; });
        if (list.empty()) {
            asks_.erase(it);
            break;
        }
    }
}

void OrderBook::simulateIncomingFlow() {
    double price = 100.0; // Base price for simulation
    // Generates randomized background orders to simulate market activity
    if (!asks_.empty() && !bids_.empty()) {
        double best_ask = asks_.begin()->first;
        double best_bid = bids_.begin()->first;
        price = 0.5 * (best_ask + best_bid); // Calculate mid-price
    }else if(asks_.empty() && !bids_.empty()) {
        price = bids_.begin()->first; // Only bids exist
    }else if(!asks_.empty() && bids_.empty()) {
        price = asks_.begin()->first; // Only asks exist
    }
    double random_price_offset = (std::rand() % 20 - 10) * 0.1;
    price = price + random_price_offset;
    int qty = 10 + (std::rand() % 90);
    bool is_buy = (std::rand() % 2 == 0);

    addLimitOrder(next_order_id_++, price, qty, is_buy);
    std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Simulate time between orders
}