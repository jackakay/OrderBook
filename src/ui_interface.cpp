#include "ui_interface.h"
#include "ImPlot/implot_internal.h" // FitThisFrame / FitPoint for the hand-drawn candlesticks

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>

#ifndef FONT_DIR
#define FONT_DIR "fonts" // CMake normally points this at the repo's fonts/ folder
#endif

namespace {

// Palette shared by every panel
const ImVec4 COL_BUY    (0.16f, 0.80f, 0.47f, 1.00f);
const ImVec4 COL_SELL   (0.94f, 0.33f, 0.31f, 1.00f);
const ImVec4 COL_ACCENT (0.26f, 0.53f, 0.96f, 1.00f);
const ImVec4 COL_TEXT   (0.84f, 0.86f, 0.90f, 1.00f);
const ImVec4 COL_DIM    (0.46f, 0.50f, 0.57f, 1.00f);
const ImVec4 COL_BG     (0.07f, 0.08f, 0.11f, 1.00f);
const ImVec4 COL_PANEL  (0.09f, 0.10f, 0.13f, 1.00f);
const ImVec4 COL_FRAME  (0.13f, 0.15f, 0.19f, 1.00f);
const ImVec4 COL_BORDER (0.17f, 0.19f, 0.24f, 1.00f);

const float LEFT_PANEL_WIDTH = 320.0f;
const float RIGHT_PANEL_WIDTH = 340.0f;

const double WINDOW_SECONDS[] = {60.0, 300.0, 900.0};
const char* WINDOW_LABELS[] = {"1 min", "5 min", "15 min"};
const double CANDLE_SECONDS[] = {1.0, 5.0, 15.0, 60.0};
const char* CANDLE_LABELS[] = {"1s", "5s", "15s", "1m"};

ImVec4 withAlpha(ImVec4 c, float a) { c.w = a; return c; }
ImU32 toU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

void formatClock(double t, char* buf, size_t size) {
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &tt);
#else
    localtime_r(&tt, &local);
#endif
    std::strftime(buf, size, "%H:%M:%S", &local);
}

bool fileExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

// Small dim uppercase heading with a line under it
void sectionHeader(const char* label, ImFont* font) {
    ImGui::Spacing();
    if (font) ImGui::PushFont(font);
    ImGui::TextColored(COL_DIM, "%s", label);
    if (font) ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();
}

// Draws a translucent quantity bar across the current table cell, then the number on top of it
void quantityCell(int qty, int max_qty, const ImVec4& color, bool right_align) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetTextLineHeight();
    float bar = max_qty > 0 ? w * static_cast<float>(qty) / static_cast<float>(max_qty) : 0.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (right_align) dl->AddRectFilled(ImVec2(p.x + w - bar, p.y), ImVec2(p.x + w, p.y + h), toU32(withAlpha(color, 0.22f)), 2.0f);
    else             dl->AddRectFilled(p, ImVec2(p.x + bar, p.y + h), toU32(withAlpha(color, 0.22f)), 2.0f);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", qty);
    if (right_align) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + w - ImGui::CalcTextSize(buf).x);
    ImGui::TextUnformatted(buf);
}

// Merges the order book's 1-second candles into candles `interval` seconds wide
std::vector<Candle> aggregateCandles(const std::deque<Candle>& base, double interval) {
    std::vector<Candle> out;
    for (const Candle& c : base) {
        double bucket = std::floor(c.time / interval) * interval;
        if (out.empty() || out.back().time != bucket) {
            Candle merged = c;
            merged.time = bucket;
            out.push_back(merged);
        } else {
            Candle& m = out.back();
            m.high = std::max(m.high, c.high);
            m.low = std::min(m.low, c.low);
            m.close = c.close;
            m.volume += c.volume;
        }
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Setup: fonts and theme
// ---------------------------------------------------------------------------

void UIInterface::setup(OrderBook& order_book) {
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // the layout is fixed, so there is nothing worth saving

    order_book.on_trade = [this](const Trade& t) {
    for (auto& o : UIInterface::my_history_)
        if (o.id == t.buy_id || o.id == t.sell_id) {
            o.filled += t.quantity;
            o.notional += toPrice(t.price) * t.quantity;
        }
    };

    // ImGui asserts on a missing font file, so check first and fall back to the built-in font
    const std::string dir = FONT_DIR;
    const std::string regular = dir + "/JetBrainsMono-Regular.ttf";
    const std::string bold = dir + "/JetBrainsMono-Bold.ttf";
    if (fileExists(regular) && fileExists(bold)) {
        font_regular_ = io.Fonts->AddFontFromFileTTF(regular.c_str(), 16.0f);
        font_bold_    = io.Fonts->AddFontFromFileTTF(bold.c_str(), 16.0f);
        font_large_   = io.Fonts->AddFontFromFileTTF(bold.c_str(), 30.0f);
        io.FontDefault = font_regular_;
    } else {
        font_regular_ = io.Fonts->AddFontDefault();
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding     = ImVec2(14, 12);
    style.FramePadding      = ImVec2(8, 5);
    style.ItemSpacing       = ImVec2(8, 7);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.CellPadding       = ImVec2(6, 3);
    style.ScrollbarSize     = 10;
    style.WindowRounding    = 0;
    style.ChildRounding     = 4;
    style.FrameRounding     = 4;
    style.PopupRounding     = 4;
    style.GrabRounding      = 4;
    style.ScrollbarRounding = 4;
    style.TabRounding       = 4;
    style.WindowBorderSize  = 1;
    style.FrameBorderSize   = 0;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                 = COL_TEXT;
    c[ImGuiCol_TextDisabled]         = COL_DIM;
    c[ImGuiCol_WindowBg]             = COL_PANEL;
    c[ImGuiCol_ChildBg]              = withAlpha(COL_PANEL, 0.0f);
    c[ImGuiCol_PopupBg]              = COL_FRAME;
    c[ImGuiCol_Border]               = COL_BORDER;
    c[ImGuiCol_FrameBg]              = COL_FRAME;
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.17f, 0.19f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.23f, 0.29f, 1.00f);
    c[ImGuiCol_TitleBg]              = COL_PANEL;
    c[ImGuiCol_TitleBgActive]        = COL_PANEL;
    c[ImGuiCol_ScrollbarBg]          = withAlpha(COL_PANEL, 0.0f);
    c[ImGuiCol_ScrollbarGrab]        = COL_BORDER;
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.27f, 0.33f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.28f, 0.31f, 0.38f, 1.00f);
    c[ImGuiCol_CheckMark]            = COL_ACCENT;
    c[ImGuiCol_SliderGrab]           = COL_ACCENT;
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.38f, 0.62f, 1.00f, 1.00f);
    c[ImGuiCol_Button]               = COL_FRAME;
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.19f, 0.22f, 0.28f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.23f, 0.27f, 0.34f, 1.00f);
    c[ImGuiCol_Header]               = withAlpha(COL_ACCENT, 0.20f);
    c[ImGuiCol_HeaderHovered]        = withAlpha(COL_ACCENT, 0.30f);
    c[ImGuiCol_HeaderActive]         = withAlpha(COL_ACCENT, 0.40f);
    c[ImGuiCol_Separator]            = COL_BORDER;
    c[ImGuiCol_TableHeaderBg]        = COL_PANEL;
    c[ImGuiCol_TableBorderStrong]    = COL_BORDER;
    c[ImGuiCol_TableBorderLight]     = withAlpha(COL_BORDER, 0.6f);
    c[ImGuiCol_TableRowBg]           = withAlpha(COL_PANEL, 0.0f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1, 1, 1, 0.025f);

    ImPlotStyle& plot = ImPlot::GetStyle();
    ImPlot::StyleColorsDark(&plot);
    plot.UseLocalTime   = true;
    plot.Use24HourClock = true;
    plot.PlotPadding    = ImVec2(6, 6);
    plot.LabelPadding   = ImVec2(4, 3);
    plot.PlotBorderSize = 0;
    plot.Colors[ImPlotCol_FrameBg]    = withAlpha(COL_PANEL, 0.0f);
    plot.Colors[ImPlotCol_PlotBg]     = COL_BG;
    plot.Colors[ImPlotCol_LegendBg]   = withAlpha(COL_FRAME, 0.85f);
    plot.Colors[ImPlotCol_AxisText]   = COL_DIM;
    plot.Colors[ImPlotCol_AxisGrid]   = withAlpha(COL_BORDER, 0.8f);
    plot.Colors[ImPlotCol_TitleText]  = COL_DIM;
    plot.Colors[ImPlotCol_Crosshairs] = COL_DIM;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void UIInterface::render(OrderBook& order_book) {
    if (!price_initialised_) {
        order_price_ = std::round(order_book.midPrice() * 100.0) / 100.0;
        price_initialised_ = true;
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 pos = vp->WorkPos;
    const ImVec2 size = vp->WorkSize;
    const float mid_width = std::max(size.x - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH, 200.0f);
    const ImGuiWindowFlags panel_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Left: market header, order entry, simulation controls, my orders
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, size.y));
    ImGui::Begin("Trading", nullptr, panel_flags);
    renderLeftPanel(order_book);
    ImGui::End();

    // Middle: candles + volume, then market depth
    ImGui::SetNextWindowPos(ImVec2(pos.x + LEFT_PANEL_WIDTH, pos.y));
    ImGui::SetNextWindowSize(ImVec2(mid_width, size.y));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, COL_BG);
    ImGui::Begin("Charts", nullptr, panel_flags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    renderCharts(order_book);
    ImGui::End();
    ImGui::PopStyleColor();

    // Right: depth ladder over the trade tape
    ImGui::SetNextWindowPos(ImVec2(pos.x + LEFT_PANEL_WIDTH + mid_width, pos.y));
    ImGui::SetNextWindowSize(ImVec2(RIGHT_PANEL_WIDTH, size.y));
    ImGui::Begin("Book", nullptr, panel_flags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    renderLadder(order_book, ImGui::GetContentRegionAvail().y * 0.58f);
    renderTrades(order_book);
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Left panel
// ---------------------------------------------------------------------------

void UIInterface::renderLeftPanel(OrderBook& order_book) {
    renderMarketHeader(order_book);
    renderOrderEntry(order_book);
    renderMyOrders(order_book);
    renderOpenPositions(order_book);
}

void UIInterface::renderMarketHeader(const OrderBook& order_book) {
    ImGui::TextColored(COL_DIM, "SIM / USD");

    double last = order_book.hasTraded() ? toPrice(order_book.lastTradePrice()) : order_book.midPrice();
    double open = order_book.hasTraded() ? toPrice(order_book.firstTradePrice()) : last;
    double change = last - open;
    const ImVec4& col = change >= 0 ? COL_BUY : COL_SELL;

    if (font_large_) ImGui::PushFont(font_large_);
    ImGui::TextColored(col, "%.2f", last);
    if (font_large_) ImGui::PopFont();
    ImGui::TextColored(col, "%+.2f (%+.2f%%)", change, open != 0.0 ? 100.0 * change / open : 0.0);
    ImGui::SameLine();
    ImGui::TextColored(COL_DIM, "since start");

    const auto& bids = order_book.getBids();
    const auto& asks = order_book.getAsks();
    if (!bids.empty() && !asks.empty()) {
        double bid = toPrice(bids.begin()->first);
        double ask = toPrice(asks.begin()->first);
        ImGui::TextColored(COL_BUY, "%.2f", bid);
        ImGui::SameLine(0, 4); ImGui::TextColored(COL_DIM, "/");
        ImGui::SameLine(0, 4); ImGui::TextColored(COL_SELL, "%.2f", ask);
        ImGui::SameLine();     ImGui::TextColored(COL_DIM, "spread %.2f", ask - bid);
    } else {
        ImGui::TextColored(COL_DIM, "One side of the book is empty");
    }
}

void UIInterface::renderOrderEntry(OrderBook& order_book) {
    sectionHeader("ORDER ENTRY", font_bold_);

    if (ImGui::RadioButton("Limit", is_limit_)) is_limit_ = true;
    ImGui::SameLine();
    if (ImGui::RadioButton("Market", !is_limit_)) is_limit_ = false;

    const float label_width = 64.0f;
    const auto& bids = order_book.getBids();
    const auto& asks = order_book.getAsks();

    // Price, with shortcuts to the touch prices (disabled for market orders)
    ImGui::BeginDisabled(!is_limit_);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(COL_DIM, "Price");
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputDouble("##price", &order_price_, 0.01, 0.10, "%.2f");
    order_price_ = std::max(order_price_, TICK_SIZE);

    ImGui::Dummy(ImVec2(label_width - ImGui::GetStyle().ItemSpacing.x, 0));
    ImGui::SameLine();
    float third = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
    if (ImGui::Button("Bid", ImVec2(third, 0)) && !bids.empty()) order_price_ = toPrice(bids.begin()->first);
    ImGui::SameLine();
    if (ImGui::Button("Mid", ImVec2(third, 0))) order_price_ = std::round(order_book.midPrice() * 100.0) / 100.0;
    ImGui::SameLine();
    if (ImGui::Button("Ask", ImVec2(third, 0)) && !asks.empty()) order_price_ = toPrice(asks.begin()->first);
    ImGui::EndDisabled();

    // Quantity, with quick sizes
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(COL_DIM, "Size");
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputInt("##qty", &order_qty_, 10, 100);
    order_qty_ = std::max(order_qty_, 1);

    ImGui::Dummy(ImVec2(label_width - ImGui::GetStyle().ItemSpacing.x, 0));
    ImGui::SameLine();
    const int quick_sizes[] = {10, 50, 100, 500};
    float quarter = (ImGui::GetContentRegionAvail().x - 3 * ImGui::GetStyle().ItemSpacing.x) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        if (i > 0) ImGui::SameLine();
        char label[16];
        std::snprintf(label, sizeof(label), "%d", quick_sizes[i]);
        if (ImGui::Button(label, ImVec2(quarter, 0))) order_qty_ = quick_sizes[i];
    }

    double est_price = is_limit_ ? order_price_ : order_book.midPrice();
    ImGui::TextColored(COL_DIM, "Est. value  $%.2f", est_price * order_qty_);

    // Big coloured BUY / SELL buttons
    ImGui::Spacing();
    float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (font_bold_) ImGui::PushFont(font_bold_);
    ImGui::PushStyleColor(ImGuiCol_Button,        withAlpha(COL_BUY, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_BUY);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  withAlpha(COL_BUY, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_Text,          COL_BG);
    if (ImGui::Button("BUY", ImVec2(half, 44))) submitOrder(order_book, true);
    ImGui::PopStyleColor(4);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        withAlpha(COL_SELL, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_SELL);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  withAlpha(COL_SELL, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_Text,          COL_BG);
    if (ImGui::Button("SELL", ImVec2(half, 44))) submitOrder(order_book, false);
    ImGui::PopStyleColor(4);
    if (font_bold_) ImGui::PopFont();

    if (last_filled_ >= 0) {
        ImGui::TextColored(last_was_buy_ ? COL_BUY : COL_SELL, "%s %d of %d", last_was_buy_ ? "Bought" : "Sold",
                           last_filled_, last_submitted_);
        if (last_filled_ < last_submitted_) {
            ImGui::SameLine();
            ImGui::TextColored(COL_DIM, is_limit_ ? "(rest is in the book)" : "(book ran dry)");
        }
    } else {
        ImGui::TextColored(COL_DIM, "Tip: click a ladder price to use it");
    }
}

void UIInterface::submitOrder(OrderBook& order_book, bool is_buy) {
    uint64_t id = order_book.nextOrderId();
    my_ids_.insert(id);
    last_submitted_ = order_qty_;
    last_was_buy_ = is_buy;

    MyOrder rec;
    rec.id = id;
    rec.is_buy = is_buy;
    rec.is_limit = is_limit_;
    rec.price = is_limit_ ? toTicks(order_price_) : 0;
    rec.quantity = order_qty_;
    rec.time = OrderBook::now();
    my_history_.push_back(rec);

    if (is_limit_) {
        // Uses exactly the price you typed: it trades against anything at that price or
        // better, and any unfilled quantity rests in the book.

        last_filled_ = order_book.addLimitOrder(id, toTicks(order_price_), order_qty_, is_buy);
        if (order_book.findOrder(id)) my_orders_.push_back(id);
    } else {
        last_filled_ = order_book.addMarketOrder(id, order_qty_, is_buy);
    }
}

void UIInterface::renderSimulationControls(OrderBook& order_book) {
    sectionHeader("SIMULATION", font_bold_);
    SimSettings& sim = order_book.sim;

    ImGui::Checkbox("Paused", &sim.paused);
    ImGui::SameLine(0, 24);
    ImGui::TextColored(COL_DIM, "%zu bid / %zu ask levels, %llu trades", order_book.getBids().size(),
                       order_book.getAsks().size(), static_cast<unsigned long long>(order_book.tradeCount()));

    // The panel sits under the wide price chart, so the sliders go side by side with their labels above
    ImGui::PushItemWidth(-96);   // leaves 96px on the right for the label
    ImGui::SliderFloat("Orders/sec", &sim.orders_per_second, 0.5f, 50.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Volatility", &sim.volatility, 0.01f, 2.0f, "$%.2f");
    ImGui::SliderFloat("Order life", &sim.order_lifetime, 5.0f, 120.0f, "%.0f s");
    ImGui::PopItemWidth();
}

std::vector<UIInterface::MyOrder> UIInterface::getMyCompletedOrders(OrderBook& order_book) {
    std::vector<MyOrder> completed_orders;
    for (const MyOrder& o : my_history_) {
        if (o.filled > 0 || o.cancelled) {
            completed_orders.push_back(o);
        }
    }
    return completed_orders;
}
std::vector<UIInterface::MyOrder> UIInterface::getMyOpenOrders(OrderBook& order_book) {
    std::vector<MyOrder> open_orders;
    for (const MyOrder& o : my_history_) {
        if (o.filled < o.quantity && !o.cancelled) {
            open_orders.push_back(o);
        }
    }
    return open_orders;
}

void UIInterface::renderMyOrders(OrderBook& order_book) {
    // Forget orders that have filled or been cancelled
    
    std::vector<UIInterface::MyOrder> open_orders = getMyOpenOrders(order_book);
    sectionHeader("MY OPEN ORDERS", font_bold_);
    if (open_orders.empty()) {
        ImGui::TextColored(COL_DIM, "None. Limit orders that don't fully\nfill will appear here.");
        return;
    }
    if (ImGui::SmallButton("Cancel all")) {
        for (UIInterface::MyOrder o : open_orders) {
            order_book.cancelOrder(o.id);
        }
        for(auto it = my_history_.begin(); it != my_history_.end();) {
            if (it->filled < it->quantity && !it->cancelled) {
                it = my_history_.erase(it);
            } else {
                ++it;
            }
        }
        return;
    }

    float height = std::max(ImGui::GetContentRegionAvail().y, 100.0f);
    if (ImGui::BeginTable("MyOrders", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH,
                          ImVec2(0, height))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Side");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Left");
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableHeadersRow();

        uint64_t to_cancel = 0;
        for (auto it = open_orders.rbegin(); it != open_orders.rend(); ++it) {
            const UIInterface::MyOrder* o = &(*it);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(o->is_buy ? COL_BUY : COL_SELL, o->is_buy ? "Buy" : "Sell");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", toPrice(o->price));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", o->quantity);
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(static_cast<int>(it->id));
            if (ImGui::SmallButton("x")) to_cancel = it->id;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel order #%llu", static_cast<unsigned long long>(it->id));
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (to_cancel) {
            order_book.cancelOrder(to_cancel); // pruned from the list next frame
            for (auto it = my_history_.begin(); it != my_history_.end(); ++it) {
                if (it->id == to_cancel) {
                    my_history_.erase(it);
                    break;
                }
            }
        }
    }
}

void UIInterface::renderOpenPositions(OrderBook& order_book) {
    sectionHeader("POSITIONS", font_bold_);

    int position = 0;    // shares held: buys add, sells subtract
    double cash = 0.0;   // dollars: buying spends, selling receives
    for (const MyOrder& o : my_history_) {
        position += o.is_buy ? o.filled : -o.filled;
        cash     += o.is_buy ? -o.notional : o.notional;
    }

    double mid = order_book.midPrice();
    double pnl = cash + position * mid;  // what you'd have if you closed out at mid

    ImGui::TextColored(COL_DIM, "Position: %d", position);
    if (position != 0)
        ImGui::TextColored(COL_DIM, "Break-even: %.2f", -cash / position);
    ImGui::TextColored(pnl >= 0 ? COL_BUY : COL_SELL, "PnL: %+.2f", pnl);
}

// ---------------------------------------------------------------------------
// Charts
// ---------------------------------------------------------------------------

void UIInterface::renderCharts(OrderBook& order_book) {
    // Toolbar
    ImGui::Checkbox("Follow live", &follow_live_);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Turns off when you drag or zoom the chart");
    ImGui::SameLine(0, 20);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(COL_DIM, "Window");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    ImGui::Combo("##window", &window_choice_, WINDOW_LABELS, IM_ARRAYSIZE(WINDOW_LABELS));
    ImGui::SameLine(0, 20);
    ImGui::TextColored(COL_DIM, "Candle");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ImGui::Combo("##candle", &candle_choice_, CANDLE_LABELS, IM_ARRAYSIZE(CANDLE_LABELS));

    // Price chart on top, simulation controls underneath
    const float sim_panel_height = 190.0f;
    float price_height = std::floor(ImGui::GetContentRegionAvail().y - sim_panel_height - ImGui::GetStyle().ItemSpacing.y);
    renderPriceChart(order_book, std::max(price_height, 150.0f));
    //renderDepthChart(order_book, ImGui::GetContentRegionAvail().y); commented out because i want my admin/simulation tab to go here
    ImGui::BeginChild("SimPanel", ImVec2(0, 0));
    if (ImGui::BeginTable("AdminArea", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);           // left half
        renderSimulationControls(order_book);

        ImGui::TableSetColumnIndex(1);           // right half
        sectionHeader("ADMIN", font_bold_);
        // ...your other settings here
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void UIInterface::renderPriceChart(const OrderBook& order_book, float height) {
    const double interval = CANDLE_SECONDS[candle_choice_];
    const double window = WINDOW_SECONDS[window_choice_];
    const double t_now = OrderBook::now();
    const std::vector<Candle> candles = aggregateCandles(order_book.getCandles(), interval);

    const ImGuiIO& io = ImGui::GetIO();
    auto setupTimeAxis = [&](ImPlotAxisFlags extra) {
        ImPlot::SetupAxis(ImAxis_X1, nullptr, extra);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        ImPlot::SetupAxisLimits(ImAxis_X1, t_now - window, t_now + window * 0.04,
                                follow_live_ ? ImPlotCond_Always : ImPlotCond_Once);
    };
    // Dragging or scrolling a chart means the user wants to look around, so stop following
    auto checkInteraction = [&]() {
        if (ImPlot::IsPlotHovered() && (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || io.MouseWheel != 0.0f))
            follow_live_ = false;
    };

    // --- Candlesticks ---
    if (ImPlot::BeginPlot("##candles", ImVec2(-1, height), ImPlotFlags_NoLegend | ImPlotFlags_NoBoxSelect)) {
        setupTimeAxis(ImPlotAxisFlags_None);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_Opposite | ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.2f");

        // Tell ImPlot how big the candles are so the Y axis fits the visible ones.
        // SetupFinish() first: FitThisFrame() is only decided once setup is locked.
        ImPlot::SetupFinish();
        if (ImPlot::FitThisFrame()) {
            for (const Candle& c : candles) {
                double xc = c.time + interval * 0.5;
                ImPlot::FitPoint(ImPlotPoint(xc, toPrice(c.low)));
                ImPlot::FitPoint(ImPlotPoint(xc, toPrice(c.high)));
            }
            if (candles.empty()) {
                ImPlot::FitPoint(ImPlotPoint(t_now, order_book.midPrice() - 0.5));
                ImPlot::FitPoint(ImPlotPoint(t_now, order_book.midPrice() + 0.5));
            }
        }

        // Best bid / ask as faint guide lines
        ImPlotSpec guide;
        guide.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit | ImPlotInfLinesFlags_Horizontal;
        if (!order_book.getBids().empty()) {
            double bid = toPrice(order_book.getBids().begin()->first);
            guide.LineColor = withAlpha(COL_BUY, 0.35f);
            ImPlot::PlotInfLines("##best_bid", &bid, 1, guide);
        }
        if (!order_book.getAsks().empty()) {
            double ask = toPrice(order_book.getAsks().begin()->first);
            guide.LineColor = withAlpha(COL_SELL, 0.35f);
            ImPlot::PlotInfLines("##best_ask", &ask, 1, guide);
        }

        ImDrawList* dl = ImPlot::GetPlotDrawList();
        ImPlot::PushPlotClipRect();
        for (const Candle& c : candles) {
            bool up = c.close >= c.open;
            ImU32 col = toU32(up ? COL_BUY : COL_SELL);
            double xc = c.time + interval * 0.5;
            ImVec2 hi = ImPlot::PlotToPixels(xc, toPrice(c.high));
            ImVec2 lo = ImPlot::PlotToPixels(xc, toPrice(c.low));
            ImVec2 body_a = ImPlot::PlotToPixels(c.time + interval * 0.15, toPrice(c.open));
            ImVec2 body_b = ImPlot::PlotToPixels(c.time + interval * 0.85, toPrice(c.close));
            ImVec2 bmin(std::min(body_a.x, body_b.x), std::min(body_a.y, body_b.y));
            ImVec2 bmax(std::max(body_a.x, body_b.x), std::max(body_a.y, body_b.y));
            if (bmax.y - bmin.y < 1.0f) bmax.y = bmin.y + 1.0f; // keep flat candles visible
            if (bmax.x - bmin.x < 1.0f) bmax.x = bmin.x + 1.0f;
            dl->AddLine(hi, lo, col, 1.0f);
            dl->AddRectFilled(bmin, bmax, col);
        }

        // Dashed line and axis tag at the last traded price
        if (order_book.hasTraded()) {
            double last = toPrice(order_book.lastTradePrice());
            bool up = candles.empty() || candles.back().close >= candles.back().open;
            ImVec4 col = up ? COL_BUY : COL_SELL;
            float y = ImPlot::PlotToPixels(0.0, last).y;
            ImVec2 p = ImPlot::GetPlotPos();
            ImVec2 s = ImPlot::GetPlotSize();
            for (float x = p.x; x < p.x + s.x; x += 9.0f)
                dl->AddLine(ImVec2(x, y), ImVec2(std::min(x + 5.0f, p.x + s.x), y), toU32(withAlpha(col, 0.8f)), 1.0f);
            ImPlot::TagY(last, col, "%.2f", last);
        }
        ImPlot::PopPlotClipRect();

        checkInteraction();
        ImPlot::EndPlot();
    }
}

void UIInterface::renderDepthChart(const OrderBook& order_book, float height) {
    // Cumulative size from the touch outwards. Bids are built best-first, then flipped so x ascends.
    std::vector<double> bid_x, bid_y, ask_x, ask_y;
    double total = 0;
    for (const auto& [level, list] : order_book.getBids()) {
        for (const auto& o : list) total += o.quantity;
        bid_x.push_back(toPrice(level));
        bid_y.push_back(total);
    }
    std::reverse(bid_x.begin(), bid_x.end());
    std::reverse(bid_y.begin(), bid_y.end());
    total = 0;
    for (const auto& [level, list] : order_book.getAsks()) {
        for (const auto& o : list) total += o.quantity;
        ask_x.push_back(toPrice(level));
        ask_y.push_back(total);
    }

    if (ImPlot::BeginPlot("Market Depth", ImVec2(-1, height), ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus)) {
        ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Opposite);
        ImPlot::SetupAxisFormat(ImAxis_X1, "%.2f");
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.0f");

        ImPlotSpec spec;
        spec.LineWeight = 1.5f;
        spec.FillAlpha = 0.25f;
        if (!bid_x.empty()) {
            // PreStep: the depth at any price between two levels is the size at the higher level and above
            spec.LineColor = COL_BUY;
            spec.FillColor = COL_BUY;
            spec.Flags = ImPlotStairsFlags_Shaded | ImPlotStairsFlags_PreStep;
            ImPlot::PlotStairs("Bids", bid_x.data(), bid_y.data(), static_cast<int>(bid_x.size()), spec);
        }
        if (!ask_x.empty()) {
            spec.LineColor = COL_SELL;
            spec.FillColor = COL_SELL;
            spec.Flags = ImPlotStairsFlags_Shaded;
            ImPlot::PlotStairs("Asks", ask_x.data(), ask_y.data(), static_cast<int>(ask_x.size()), spec);
        }

        double mid = order_book.midPrice();
        ImPlotSpec mid_line;
        mid_line.LineColor = withAlpha(COL_DIM, 0.6f);
        mid_line.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit;
        ImPlot::PlotInfLines("##mid", &mid, 1, mid_line);
        ImPlot::EndPlot();
    }
}

// ---------------------------------------------------------------------------
// Right panel
// ---------------------------------------------------------------------------

bool UIInterface::levelHasMyOrder(const std::list<Order>& level) const {
    for (const auto& o : level)
        if (my_ids_.count(o.id)) return true;
    return false;
}

void UIInterface::renderLadder(const OrderBook& order_book, float height) {
    ImGui::BeginChild("LadderPanel", ImVec2(0, height));
    sectionHeader("ORDER BOOK", font_bold_);

    // Fit as many levels per side as the panel has room for
    const float row_height = ImGui::GetTextLineHeight() + 2.0f * ImGui::GetStyle().CellPadding.y;
    const float rows_available = ImGui::GetContentRegionAvail().y / row_height - 2.0f; // header + spread
    const int levels = std::clamp(static_cast<int>(rows_available / 2.0f), 3, 20);

    struct Row { Price price; int qty; bool mine; };
    std::vector<Row> asks, bids;
    int max_qty = 1;
    for (const auto& [level, list] : order_book.getAsks()) {
        if (static_cast<int>(asks.size()) == levels) break;
        int qty = 0;
        for (const auto& o : list) qty += o.quantity;
        asks.push_back({level, qty, levelHasMyOrder(list)});
        max_qty = std::max(max_qty, qty);
    }
    for (const auto& [level, list] : order_book.getBids()) {
        if (static_cast<int>(bids.size()) == levels) break;
        int qty = 0;
        for (const auto& o : list) qty += o.quantity;
        bids.push_back({level, qty, levelHasMyOrder(list)});
        max_qty = std::max(max_qty, qty);
    }

    // Price cell: click to load the price into the order ticket; a dot marks levels holding your orders
    auto priceCell = [&](const Row& row, const ImVec4& col) {
        char label[48];
        std::snprintf(label, sizeof(label), "%.2f##%lld", toPrice(row.price), static_cast<long long>(row.price));
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        if (ImGui::Selectable(label)) {
            order_price_ = toPrice(row.price);
            is_limit_ = true;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to use %.2f as your limit price", toPrice(row.price));
        if (row.mine) {
            ImVec2 r = ImGui::GetItemRectMin();
            float cy = r.y + ImGui::GetItemRectSize().y * 0.5f;
            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(r.x + 4.0f, cy), 3.0f, toU32(COL_ACCENT));
        }
    };

    if (ImGui::BeginTable("Ladder", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Bid size");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Ask size");
        ImGui::TableHeadersRow();

        // Asks: furthest at the top, best ask just above the spread
        for (int i = static_cast<int>(asks.size()) - 1; i >= 0; --i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(1);
            priceCell(asks[i], COL_SELL);
            ImGui::TableSetColumnIndex(2);
            quantityCell(asks[i].qty, max_qty, COL_SELL, false);
        }

        // Spread row
        ImGui::TableNextRow();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, toU32(COL_FRAME));
        ImGui::TableSetColumnIndex(1);
        if (!asks.empty() && !bids.empty()) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f", toPrice(asks.front().price - bids.front().price));
            float w = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - ImGui::CalcTextSize(buf).x) * 0.5f);
            ImGui::TextColored(COL_DIM, "%s", buf);
        } else {
            ImGui::TextColored(COL_DIM, "--");
        }
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(COL_DIM, "spread");

        // Bids: best bid just below the spread
        for (const Row& row : bids) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            quantityCell(row.qty, max_qty, COL_BUY, true);
            ImGui::TableSetColumnIndex(1);
            priceCell(row, COL_BUY);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void UIInterface::renderTrades(const OrderBook& order_book) {
    sectionHeader("RECENT TRADES", font_bold_);

    if (ImGui::BeginTable("Trades", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchSame,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Size");
        ImGui::TableHeadersRow();

        for (const Trade& t : order_book.getRecentTrades()) {
            bool mine = my_ids_.count(t.buy_id) || my_ids_.count(t.sell_id);
            const ImVec4& col = t.buyer_was_aggressor ? COL_BUY : COL_SELL;
            if (mine && font_bold_) ImGui::PushFont(font_bold_);

            ImGui::TableNextRow();
            if (mine) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, toU32(withAlpha(COL_ACCENT, 0.15f)));
            ImGui::TableSetColumnIndex(0);
            char clock[16];
            formatClock(t.time, clock, sizeof(clock));
            ImGui::TextColored(COL_DIM, "%s", clock);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(col, "%.2f", toPrice(t.price));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d%s", t.quantity, mine ? " *" : "");

            if (mine && font_bold_) ImGui::PopFont();
        }
        ImGui::EndTable();
    }
}
