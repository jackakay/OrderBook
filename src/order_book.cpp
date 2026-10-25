#include "order_book.h"
#include "matching_engine.h"

#include <algorithm>
#include <chrono>
#include <iterator>

OrderBook::OrderBook() : rng_(std::random_device{}()) {
    last_update_ = now();
}

double OrderBook::now() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

int OrderBook::addLimitOrder(uint64_t id, Price price, int quantity, bool is_buy) {
    if (quantity <= 0 || price <= 0) return 0;

    double t = now();
    int remaining;
    if (is_buy) {
        // A buy crosses any ask priced at or below its limit
        remaining = MatchingEngine::match(asks_, quantity,
            [price](Price level) { return level <= price; },
            [&](const Order& maker, int fill, bool maker_done) {
                recordTrade({id, maker.id, maker.price, fill, true, t});
                if (maker_done) index_.erase(maker.id);
            });
        // Rest whatever didn't trade on the bid side
        if (remaining > 0) {
            auto& queue = bids_[price];
            queue.push_back({id, price, remaining, true, t});
            index_[id] = {true, price, std::prev(queue.end())};
        }
    } else {
        // A sell crosses any bid priced at or above its limit
        remaining = MatchingEngine::match(bids_, quantity,
            [price](Price level) { return level >= price; },
            [&](const Order& maker, int fill, bool maker_done) {
                recordTrade({maker.id, id, maker.price, fill, false, t});
                if (maker_done) index_.erase(maker.id);
            });
        // Rest whatever didn't trade on the ask side
        if (remaining > 0) {
            auto& queue = asks_[price];
            queue.push_back({id, price, remaining, false, t});
            index_[id] = {false, price, std::prev(queue.end())};
        }
    }
    return quantity - remaining;
}

int OrderBook::addMarketOrder(uint64_t id, int quantity, bool is_buy) {
    if (quantity <= 0) return 0;

    // A market order takes any price; whatever can't be filled is dropped, never rested
    double t = now();
    auto any_price = [](Price) { return true; };
    int remaining;
    if (is_buy) {
        remaining = MatchingEngine::match(asks_, quantity, any_price,
            [&](const Order& maker, int fill, bool maker_done) {
                recordTrade({id, maker.id, maker.price, fill, true, t});
                if (maker_done) index_.erase(maker.id);
            });
    } else {
        remaining = MatchingEngine::match(bids_, quantity, any_price,
            [&](const Order& maker, int fill, bool maker_done) {
                recordTrade({maker.id, id, maker.price, fill, false, t});
                if (maker_done) index_.erase(maker.id);
            });
    }
    return quantity - remaining;
}

bool OrderBook::cancelOrder(uint64_t id) {
    auto found = index_.find(id);
    if (found == index_.end()) return false; // unknown, or already filled

    const Location& loc = found->second;
    if (loc.is_buy) {
        auto level = bids_.find(loc.price);
        level->second.erase(loc.it);
        if (level->second.empty()) bids_.erase(level);
    } else {
        auto level = asks_.find(loc.price);
        level->second.erase(loc.it);
        if (level->second.empty()) asks_.erase(level);
    }
    index_.erase(found);
    return true;
}

const Order* OrderBook::findOrder(uint64_t id) const {
    auto found = index_.find(id);
    return found == index_.end() ? nullptr : &*found->second.it;
}

double OrderBook::midPrice() const {
    if (!asks_.empty() && !bids_.empty()) {
        return 0.5 * (toPrice(asks_.begin()->first) + toPrice(bids_.begin()->first));
    } else if (!bids_.empty()) {
        return toPrice(bids_.begin()->first); // Only bids exist
    } else if (!asks_.empty()) {
        return toPrice(asks_.begin()->first); // Only asks exist
    }
    return 100.0; // Base price for an empty book
}

void OrderBook::update() {
    double t = now();
    double dt = t - last_update_;
    last_update_ = t;
    if (sim.paused) return;

    // Place orders at the configured rate, independent of the frame rate.
    // Capped so a long stall (e.g. dragging the window) doesn't dump thousands at once.
    order_accumulator_ += dt * sim.orders_per_second;
    int to_place = std::min(static_cast<int>(order_accumulator_), 100);
    order_accumulator_ -= static_cast<int>(order_accumulator_);
    for (int i = 0; i < to_place; ++i) simulateIncomingFlow();

    // Cancel simulated orders that have rested longer than their lifetime, so the book stays a sensible size
    while (!sim_orders_.empty() && t - sim_orders_.front().first > sim.order_lifetime) {
        cancelOrder(sim_orders_.front().second); // no-op if it already filled
        sim_orders_.pop_front();
    }
}

void OrderBook::simulateIncomingFlow() {
    // Generates randomized background orders around the mid-price to simulate market activity.
    // The offset is symmetric so the price has no built-in drift.
    int max_offset = std::max(1, static_cast<int>(std::lround(sim.volatility / TICK_SIZE)));
    std::uniform_int_distribution<int> offset_ticks(-max_offset, max_offset);
    std::uniform_int_distribution<int> qty_dist(10, 99);
    std::bernoulli_distribution side(0.5);

    Price price = toTicks(midPrice()) + offset_ticks(rng_);
    if (price < 1) price = 1;

    uint64_t id = nextOrderId();
    addLimitOrder(id, price, qty_dist(rng_), side(rng_));
    if (index_.count(id)) sim_orders_.push_back({now(), id}); // it rested, so it can expire later
}

void OrderBook::recordTrade(const Trade& trade) {
    recent_trades_.push_front(trade);
    if (recent_trades_.size() > MAX_RECENT_TRADES) recent_trades_.pop_back();

    if (trade_count_ == 0) first_trade_price_ = trade.price;
    last_trade_price_ = trade.price;
    ++trade_count_;

    // Roll the trade into the current 1-second candle
    double second = std::floor(trade.time);
    if (candles_.empty() || candles_.back().time != second) {
        candles_.push_back({second, trade.price, trade.price, trade.price, trade.price, trade.quantity});
        if (candles_.size() > MAX_CANDLES) candles_.pop_front();
    } else {
        Candle& c = candles_.back();
        c.high = std::max(c.high, trade.price);
        c.low = std::min(c.low, trade.price);
        c.close = trade.price;
        c.volume += trade.quantity;
    }

    if (on_trade) on_trade(trade);
}
