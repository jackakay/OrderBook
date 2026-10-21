#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include <map>
#include <list>
#include <string>
#include <cstdint>
#include <thread>   
#include <chrono>
#include <vector>
#include <cmath>

struct Order {
    uint64_t id;
    double price;
    int quantity;
    bool is_buy; // true for buy (bid), false for sell (ask)
};

#include <vector>
#include <cmath>

template <typename T, size_t Capacity>
struct RollingBuffer {
    size_t offset = 0;
    size_t size = 0;
    std::vector<T> data;

    RollingBuffer() {
        data.resize(Capacity);
    }

    void AddPoint(T x) {
        if (size < Capacity) {
            data[size++] = x;
        } else {
            data[offset] = x;
            offset = (offset + 1) % Capacity;
        }
    }

    void Clear() {
        offset = 0;
        size = 0;
    }
};

// A simple structure to hold an (X, Y) coordinate pair (e.g., Timestamp vs Price)
struct TimePricePoint {
    double x; // timestamp
    double y; // price
};

class OrderBook {
public:
    void addLimitOrder(uint64_t id, double price, int quantity, bool is_buy);
    void cancelOrder(uint64_t id);
    void simulateIncomingFlow();
    void addMarketOrder(uint64_t id, int quantity, bool is_buy);
    // Getters for UI display
    const std::map<double, std::list<Order>, std::greater<double>>& getBids() const { return bids_; }
    const std::map<double, std::list<Order>, std::less<double>>& getAsks() const { return asks_; }

private:
    // Bids: Sorted highest price first
    std::map<double, std::list<Order>, std::greater<double>> bids_;
    
    // Asks: Sorted lowest price first
    std::map<double, std::list<Order>, std::less<double>> asks_;
    
    uint64_t next_order_id_ = 1;
};

#endif // ORDER_BOOK_H