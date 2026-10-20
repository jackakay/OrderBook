#include "ui_interface.h"
#include <imgui.h>

void UIInterface::render(OrderBook& order_book) {
    ImGui::Begin("Limit Order Book & Trading Interface");

    // Trading Control Panel
    static float order_price = 100.0f;
    static int order_qty = 50;
    static bool is_buy = true;

    ImGui::Text("Manual Order Entry");
    
    // Fixed Radio Buttons with proper inline layout
    if (ImGui::RadioButton("Buy (Bid)", is_buy == true)) {
        is_buy = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Sell (Ask)", is_buy == false)) {
        is_buy = false;
    }
    
    ImGui::InputFloat("Price", &order_price);
    ImGui::InputInt("Quantity", &order_qty);

    if (ImGui::Button("Submit Order")) {
        static uint64_t manual_id = 10000;
        order_book.addLimitOrder(manual_id++, order_price, order_qty, is_buy);
    }

    double price = 100.0; // In case the order book is empty

    
    if (!order_book.getAsks().empty() && !order_book.getBids().empty()) {
        double best_ask = order_book.getAsks().begin()->first;
        double best_bid = order_book.getBids().begin()->first;
        price = 0.5 * (best_ask + best_bid); // Calculate mid-price
    }

    ImGui::Text("Current price: %.2f", price);

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