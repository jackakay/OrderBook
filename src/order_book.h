#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include <map>
#include <list>
#include <string>
#include <cstdint>
#include <thread>   
#include <chrono>

struct Order {
    uint64_t id;
    double price;
    int quantity;
    bool is_buy; // true for buy (bid), false for sell (ask)
};

class OrderBook {
public:
    void addLimitOrder(uint64_t id, double price, int quantity, bool is_buy);
    void cancelOrder(uint64_t id);
    void simulateIncomingFlow();

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