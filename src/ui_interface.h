#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include "order_book.h"
#include "imgui.h"

class UIInterface {
public:
    void render(OrderBook& order_book);
};

#endif // UI_INTERFACE_H