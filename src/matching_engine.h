#ifndef MATCHING_ENGINE_H
#define MATCHING_ENGINE_H

#include <list>
#include <cstdint>

#include "order_book.h"

namespace MatchingEngine {

// Matches an incoming order against one side of the book using price-time priority:
// best price level first, and oldest order first within a level.
//
// `levels` is the opposite side (asks for a buy, bids for a sell), already sorted best-first.
// `crosses(level_price)` says whether the incoming order is willing to trade at that level;
// a market order simply always returns true.
// `on_fill(maker, fill_qty, maker_done)` is called for every fill, before a fully filled
// maker is removed from the book.
//
// Returns the quantity of the incoming order that is left unfilled.
template <typename Levels, typename CrossFn, typename FillFn>
int match(Levels& levels, int quantity, CrossFn crosses, FillFn on_fill) {
    while (quantity > 0 && !levels.empty() && crosses(levels.begin()->first)) {
        auto level = levels.begin();
        std::list<Order>& queue = level->second;
        Order& maker = queue.front();

        int fill = (maker.quantity <= quantity) ? maker.quantity : quantity;
        maker.quantity -= fill;
        quantity -= fill;

        bool maker_done = (maker.quantity == 0);
        on_fill(maker, fill, maker_done);

        if (maker_done) {
            queue.pop_front();
            if (queue.empty()) {
                levels.erase(level);
            }
        }
    }
    return quantity;
}

} // namespace MatchingEngine

#endif // MATCHING_ENGINE_H
