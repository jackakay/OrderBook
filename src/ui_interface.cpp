#include "ui_interface.h"
const int MAX_HISTORY_SIZE = 10000;
static RollingBuffer<TimePricePoint, MAX_HISTORY_SIZE> history;

void UIInterface::plot_graph() {
    if (ImPlot::BeginPlot("Live Stock Price", ImVec2(-1, 300))) {
        // Pass ImPlotAxisFlags_AutoFit to the Y-axis. 
        // ImPlot's internal C code calculates this natively and extremely fast.
        ImPlot::SetupAxes("Time", "Price ($)", ImPlotFlags_None, ImPlotAxisFlags_AutoFit);

        if (history.size > 0) {
            // 1. Control ONLY the X-axis rolling window (fast math, no loops)
            size_t latestIdx = (history.offset + history.size - 1) % MAX_HISTORY_SIZE;
            double latestX = history.data[latestIdx].x;
            ImPlot::SetupAxisLimits(ImAxis_X1, std::max(latestX - 100.0,0.0), latestX + 5.0, ImPlotCond_Always);

            // 2. Render using the zero-copy getter callback
            ImPlot::PlotLineG("AAPL", [](int idx, void* data) {
                auto* buffer = static_cast<RollingBuffer<TimePricePoint, MAX_HISTORY_SIZE>*>(data);
                size_t actualIdx = (buffer->size < MAX_HISTORY_SIZE) ? idx : (buffer->offset + idx) % MAX_HISTORY_SIZE;
                return ImPlotPoint(buffer->data[actualIdx].x, buffer->data[actualIdx].y);
            }, &history, history.size);
        }

        ImPlot::EndPlot();
    }
}
void UIInterface::addTickData(double timestamp, double price) {
    history.AddPoint({timestamp, price});
}

void UIInterface::render(OrderBook& order_book) {
    ImGui::Begin("Limit Order Book & Trading Interface");

    // Trading Control Panel
    static float order_price = 100.0f;
    static int order_qty = 50;
    static bool is_buy = true;
    static bool is_limit = true;

    ImGui::Text("Manual Order Entry");
    
    // Fixed Radio Buttons with proper inline layout
    if (ImGui::RadioButton("Buy (Bid)", is_buy == true)) {
        is_buy = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Sell (Ask)", is_buy == false)) {
        is_buy = false;
    }

    
    if(ImGui::RadioButton("Market Order", is_limit == false)) {
        is_limit = !is_limit;
    }

    if(is_limit) ImGui::InputFloat("Price", &order_price);
    ImGui::InputInt("Quantity", &order_qty);

    if (ImGui::Button("Submit Order")) {
        static uint64_t manual_id = 10000;
        if (is_limit) { // If it's a market/aggressive order
            if (is_buy) {
                // Match or exceed the best ask to buy immediately
                order_price = order_book.getAsks().empty() ? order_price : order_book.getAsks().begin()->first; 
            } else {
                // Match or undercut the best bid to sell immediately
                order_price = order_book.getBids().empty() ? order_price : order_book.getBids().begin()->first; 
            }
            order_book.addLimitOrder(manual_id++, order_price, order_qty, is_buy);
        }
        else {
            order_book.addMarketOrder(manual_id++, order_qty, is_buy);
        }
    }
    
    double price = 100.0; // In case the order book is empty

    
    if (!order_book.getAsks().empty() && !order_book.getBids().empty()) {
        double best_ask = order_book.getAsks().begin()->first;
        double best_bid = order_book.getBids().begin()->first;
        price = 0.5 * (best_ask + best_bid); // Calculate mid-price
    }else if(order_book.getAsks().empty() && !order_book.getBids().empty()) {
        price = order_book.getBids().begin()->first; // Only bids exist
    }else if(!order_book.getAsks().empty() && order_book.getBids().empty()) {
        price = order_book.getAsks().begin()->first; // Only asks exist
    }
    addTickData(ImGui::GetTime(), price); // Add the current price to the rolling buffer for plotting

    ImGui::Text("Current price: %.2f", price);


    ImGui::Text("Total quantity of bids: %d", get_total_bids(order_book));
    ImGui::SameLine();
    ImGui::Text("Total quantity of asks: %d", get_total_asks(order_book));
    ImGui::Separator();
    
    // Order Book Display Tables
    ImGui::Text("Order Book Depth");
    
    if (ImGui::BeginTable("OrderBookTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Bids (Buy)");
        ImGui::TableSetupColumn("Asks (Sell)");
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Price | Qty");
        for (const auto& [price, list] : order_book.getBids()) {
            int total_qty = 0;
            for (const auto& o : list) total_qty += o.quantity;
            ImGui::Text("%.2f : %d", price, total_qty);
        }

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Price | Qty");
        for (const auto& [price, list] : order_book.getAsks()) {
            int total_qty = 0;
            for (const auto& o : list) total_qty += o.quantity;
            ImGui::Text("%.2f : %d", price, total_qty);
        }

        ImGui::EndTable();
    }
    
    ImGui::End();
}



int UIInterface::get_total_bids(const OrderBook& order_book) {
    int total_qty = 0;
    for(auto& [price, list] : order_book.getBids()) {
        
        for (const auto& o : list) total_qty += o.quantity;
        
    }
    return total_qty;
}
int UIInterface::get_total_asks(const OrderBook& order_book) {
    int total_qty = 0;
    for(auto& [price, list] : order_book.getAsks()) {
        
        for (const auto& o : list) total_qty += o.quantity;
        
    }
    return total_qty;
}