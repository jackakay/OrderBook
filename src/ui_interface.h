#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include "order_book.h"
#include "imgui.h"
#include "ImPlot/implot.h"

class UIInterface {
public:
    void render(OrderBook& order_book);
    void plot_graph();
private:
    void addTickData(double timestamp, double price);
    int get_total_bids(const OrderBook& order_book);
    int get_total_asks(const OrderBook& order_book);
};


#endif // UI_INTERFACE_H