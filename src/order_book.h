#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include <map>
#include <list>
#include <deque>
#include <unordered_map>
#include <random>
#include <utility>
#include <cstdint>
#include <cmath>
#include <functional>

// Prices are stored as integer ticks (1 tick = $0.01) so that two orders at the
// "same" price always land on the same level. Doubles are only used for display.
using Price = int64_t;
constexpr double TICK_SIZE = 0.01;

inline Price toTicks(double price) { return static_cast<Price>(std::llround(price / TICK_SIZE)); }
inline double toPrice(Price ticks) { return static_cast<double>(ticks) * TICK_SIZE; }



struct Order {
    uint64_t id;
    Price price;
    int quantity;
    bool is_buy; // true for buy (bid), false for sell (ask)
    double time; // when it was placed (seconds since the Unix epoch)
};

struct Trade {
    uint64_t buy_id;
    uint64_t sell_id;
    Price price;   // always the resting (maker) order's price
    int quantity;
    bool buyer_was_aggressor;
    double time;
};

// One second of trading. The UI merges these into wider candles for display.
struct Candle {
    double time; // start of the second
    Price open, high, low, close;
    int volume;
};

// Knobs for the background order flow, changed live from the UI.
struct SimSettings {
    bool paused = false;
    float orders_per_second = 5.0f;
    float volatility = 0.50f;      // max distance from the mid-price, in dollars
    float order_lifetime = 30.0f;  // simulated orders are cancelled after this many seconds
};

using BidLevels = std::map<Price, std::list<Order>, std::greater<Price>>; // highest price first
using AskLevels = std::map<Price, std::list<Order>, std::less<Price>>;    // lowest price first

class OrderBook {
public:
    OrderBook();

    // Seconds since the Unix epoch, so ImPlot can show a real clock on the time axis.
    static double now();

    // Both return the quantity that traded immediately.
    int addLimitOrder(uint64_t id, Price price, int quantity, bool is_buy);
    int addMarketOrder(uint64_t id, int quantity, bool is_buy);
    bool cancelOrder(uint64_t id);

    // Call once per frame: places simulated orders at the configured rate and expires old ones.
    void update();
    void simulateIncomingFlow();

    // Hands out ids so manual and simulated orders never collide.
    uint64_t nextOrderId() { return next_order_id_++; }

    SimSettings sim;

    std::function<void(const Trade&)> on_trade;// Called whenever a trade occurs, for the UI to update its charts.

    // Getters for UI display
    const BidLevels& getBids() const { return bids_; }
    const AskLevels& getAsks() const { return asks_; }
    const std::deque<Trade>& getRecentTrades() const { return recent_trades_; } // newest first
    const std::deque<Candle>& getCandles() const { return candles_; }          // oldest first
    const Order* findOrder(uint64_t id) const; // nullptr once filled or cancelled
    bool hasTraded() const { return trade_count_ > 0; }
    Price lastTradePrice() const { return last_trade_price_; }
    Price firstTradePrice() const { return first_trade_price_; }
    uint64_t tradeCount() const { return trade_count_; }
    double midPrice() const; // falls back to one side, then to $100 if the book is empty

private:
    void recordTrade(const Trade& trade);

    BidLevels bids_;
    AskLevels asks_;

    // Where each resting order lives, so cancels don't scan the whole book.
    struct Location {
        bool is_buy;
        Price price;
        std::list<Order>::iterator it;
    };
    std::unordered_map<uint64_t, Location> index_;

    static constexpr size_t MAX_RECENT_TRADES = 200;
    static constexpr size_t MAX_CANDLES = 3600; // one hour of 1-second candles
    std::deque<Trade> recent_trades_;
    std::deque<Candle> candles_;
    Price last_trade_price_ = 0;
    Price first_trade_price_ = 0;
    uint64_t trade_count_ = 0;

    // Simulated orders in the order they were placed, for expiry
    std::deque<std::pair<double, uint64_t>> sim_orders_;
    double last_update_ = 0.0;
    double order_accumulator_ = 0.0;

    uint64_t next_order_id_ = 1;
    std::mt19937 rng_;
};

#endif // ORDER_BOOK_H
