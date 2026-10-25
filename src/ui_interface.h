#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include <vector>
#include <unordered_set>

#include "order_book.h"
#include "imgui.h"
#include "ImPlot/implot.h"

class UIInterface {

    struct MyOrder {
        uint64_t id; bool is_buy, is_limit; Price price; int quantity;
        int filled = 0; double notional = 0; // sum of price*qty, for average fill price
        double time; bool cancelled = false;
    };
    
public:
    // Loads fonts and applies the theme. Call after the ImGui/ImPlot contexts exist, before the first frame.
    void setup(OrderBook& order_book);
    // Draws the whole fixed layout: order entry | charts | depth ladder and trades.
    void render(OrderBook& order_book);
    

private:
    void renderLeftPanel(OrderBook& order_book);
    void renderMarketHeader(const OrderBook& order_book);
    void renderOrderEntry(OrderBook& order_book);
    void renderSimulationControls(OrderBook& order_book);
    void renderMyOrders(OrderBook& order_book);
    void renderOpenPositions(OrderBook& order_book);

    void renderCharts(OrderBook& order_book);
    void renderPriceChart(const OrderBook& order_book, float height);
    void renderDepthChart(const OrderBook& order_book, float height);

    void renderLadder(const OrderBook& order_book, float height);
    void renderTrades(const OrderBook& order_book);

    void submitOrder(OrderBook& order_book, bool is_buy);
    bool levelHasMyOrder(const std::list<Order>& level) const;

    std::vector<UIInterface::MyOrder> getMyCompletedOrders(OrderBook& order_book);
    std::vector<UIInterface::MyOrder> getMyOpenOrders(OrderBook& order_book);

    // Fonts (nullptr falls back to ImGui's built-in font)
    ImFont* font_regular_ = nullptr;
    ImFont* font_bold_ = nullptr;
    ImFont* font_large_ = nullptr;

    // Order entry
    bool price_initialised_ = false;
    double order_price_ = 100.0;
    int order_qty_ = 50;
    bool is_limit_ = true;
    int last_filled_ = -1; // -1 until the first manual order
    int last_submitted_ = 0;
    bool last_was_buy_ = true;

    // Orders placed from the UI, newest last
    std::vector<uint64_t> my_orders_;
    std::unordered_set<uint64_t> my_ids_;
    std::vector<MyOrder> my_history_;
    

    // Chart settings
    bool follow_live_ = true;
    int window_choice_ = 1; // index into WINDOW_SECONDS
    int candle_choice_ = 1; // index into CANDLE_SECONDS
};


#endif // UI_INTERFACE_H
