#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <algorithm>

// --- Structs (Updated) ---
struct PairConfig {
    std::string ticker;
    std::string interval;
    int lookback;
};

struct Candle {
    std::string datetime;
    double open, high, low, close;
};

struct StrategyParams {
    int sma_short = 5;
    int sma_long = 20;
    int rsi_period = 14;
    int atr_period = 14;
    double performance = -1e9; // Performance score
};


// --- Helper & Indicator Functions (Updated for indexed calculation) ---

std::vector<PairConfig> readConfig(const std::string& file); // Assume full function is here
std::vector<Candle> readData(const std::string& file);       // Assume full function is here

double computeSMA();

double computeRSI(const std::vector<double>& closes, size_t end_index, int period);

double computeTrueRange(const Candle& current, const Candle& previous);

double computeATR(const std::vector<Candle>& candles, size_t end_index, int period);




double simulateBacktest(const std::vector<Candle>& candles, const StrategyParams& params);

StrategyParams findBestParameters(const std::vector<Candle>& historical_candles);


// --- Main Loop (Updated) ---

int main(int argc, char* argv[]) {
    if (argc < 2) { /* error handling */ return 1; }

    std::vector<PairConfig> cfgs = readConfig(argv[1]);
    if (cfgs.empty()) { /* error handling */ return 1; }

    for (const auto& cfg : cfgs) {
        std::cout << "\n======================================================\n"
        << "Processing: " << cfg.ticker << " (" << cfg.interval << ")"
        << "\n======================================================\n";

        std::vector<Candle> candles = readData(cfg.ticker + ".csv");
        if (candles.size() < 100) {
            std::cerr << "Not enough data for optimization. Need at least 100. Found: " << candles.size() << std::endl;
            continue;
        }

        std::vector<Candle> optimization_candles(candles.begin(), candles.end() - 1);
        StrategyParams optimal_params = findBestParameters(optimization_candles);

        std::cout << "\n--- Optimization Complete for " << cfg.ticker << " ---\n"
        << "Optimal Params Found: SMA(" << optimal_params.sma_short << "/" << optimal_params.sma_long
        << "), RSI(" << optimal_params.rsi_period << "), ATR(" << optimal_params.atr_period << ")\n\n";

        std::vector<double> closes;
        for (const auto& c : candles) closes.push_back(c.close);

        // --- Generate final signal using ALL optimal parameters ---
        double sma_short = computeSMA(closes, closes.size() - 1, optimal_params.sma_short);
        double sma_long = computeSMA(closes, closes.size() - 1, optimal_params.sma_long);
        double rsi = computeRSI(closes, closes.size() - 1, optimal_params.rsi_period);
        double atr = computeATR(candles, candles.size() - 1, optimal_params.atr_period);

        std::string signal = "HOLD";
        double entry = candles.back().close;
        double sl = 0, tp = 0;

        if (sma_short > sma_long && rsi > 50 && sma_short > 0) {
            signal = "BUY";
            sl = entry - 1.5 * atr;
            tp = entry + 2.0 * atr;
        } else if (sma_short < sma_long && rsi < 50 && sma_short > 0) {
            signal = "SELL";
            sl = entry + 1.5 * atr;
            tp = entry - 2.0 * atr;
        }

        std::cout << "--- FINAL SIGNAL ---\n"
        << candles.back().datetime << " | " << cfg.ticker << " | " << signal
        << " | Entry=" << entry;
        if (signal != "HOLD") std::cout << " SL=" << sl << " TP=" << tp;
        std::cout << " | Using: SMA" << optimal_params.sma_short << "/" << optimal_params.sma_long
        << " RSI" << optimal_params.rsi_period << " ATR" << optimal_params.atr_period << "\n";
    }

    return 0;
}

std::vector<PairConfig> readConfig(const std::string& file) {
    std::vector<PairConfig> cfgs;
    std::ifstream f(file);
    std::string line;
    while(getline(f, line)) {
        if(line.empty() || line[0]=='#') continue;
        std::stringstream ss(line);
        PairConfig p;
        ss >> p.ticker >> p.interval >> p.lookback;
        cfgs.push_back(p);
    }
    return cfgs;
}

std::vector<Candle> readData(const std::string& file) {
    std::vector<Candle> candles;
    std::ifstream f(file);
    std::string line;
    getline(f, line); // Skip header
    while(getline(f, line)) {
        std::stringstream ss(line);
        std::string datetime, open, high, low, close, adj_close, volume;
        getline(ss, datetime, ',');
        getline(ss, open, ',');
        getline(ss, high, ',');
        getline(ss, low, ',');
        getline(ss, close, ',');
        getline(ss, adj_close, ',');
        getline(ss, volume, ',');
        if(open.empty() || high.empty() || low.empty() || close.empty()) continue;
        try {
            candles.push_back({datetime, std::stod(open), std::stod(high), std::stod(low), std::stod(close)});
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << " -> " << e.what() << std::endl;
        }
    }
    return candles;
}


double computeSMA(const std::vector<double>& prices, size_t end_index, int period) {
    if (end_index + 1 < period || period <= 0) return 0;
    double sum = 0.0;
    for (size_t i = end_index - period + 1; i <= end_index; ++i) {
        sum += prices[i];
    }
    return sum / period;
}

double computeRSI(const std::vector<double>& closes, size_t end_index, int period) {
    if (end_index + 1 < period + 1) return 0;

    double gain_sum = 0.0;
    double loss_sum = 0.0;

    for (size_t i = end_index - period + 1; i <= end_index; ++i) {
        double change = closes[i] - closes[i-1];
        if (change > 0) {
            gain_sum += change;
        } else {
            loss_sum += -change;
        }
    }

    double avg_gain = gain_sum / period;
    double avg_loss = loss_sum / period;

    if (avg_loss == 0) return 100.0;

    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

double computeTrueRange(const Candle& current, const Candle& previous) {
    double tr1 = current.high - current.low;
    double tr2 = std::abs(current.high - previous.close);
    double tr3 = std::abs(current.low - previous.close);
    return std::max({tr1, tr2, tr3});
}


double computeATR(const std::vector<Candle>& candles, size_t end_index, int period) {
    if (end_index + 1 < period + 1) return 0;
    double tr_sum = 0.0;
    for (size_t i = end_index - period + 1; i <= end_index; ++i) {
        tr_sum += computeTrueRange(candles[i], candles[i-1]);
    }
    return tr_sum / period;
}

double simulateBacktest(const std::vector<Candle>& candles, const StrategyParams& params) {
    if (candles.size() < std::max(params.sma_long, params.rsi_period) + 1) return -1e9;

    std::vector<double> closes;
    for(const auto& c : candles) closes.push_back(c.close);

    double profit = 0.0;
    bool in_position = false; // false = none, true = long
    double entry_price = 0.0;
    double stop_loss = 0.0;

    size_t start_index = std::max({params.sma_long, params.rsi_period, params.atr_period}) + 1;

    for (size_t i = start_index; i < candles.size(); ++i) {
        // Check if stop loss was hit
        if (in_position) {
            if (candles[i].low <= stop_loss) {
                profit += (stop_loss - entry_price);
                in_position = false;
            }
        }

        double sma_short = computeSMA(closes, i, params.sma_short);
        double sma_long = computeSMA(closes, i, params.sma_long);
        double rsi = computeRSI(closes, i, params.rsi_period);

        // Entry Logic
        if (!in_position && sma_short > sma_long && rsi > 50) {
            in_position = true;
            entry_price = closes[i];
            double atr = computeATR(candles, i, params.atr_period);
            stop_loss = entry_price - 1.5 * atr;
        }
        // Exit Logic
        else if (in_position && sma_short < sma_long) {
            profit += (closes[i] - entry_price);
            in_position = false;
        }
    }
    return profit;
}

StrategyParams findBestParameters(const std::vector<Candle>& historical_candles) {
    StrategyParams best_params;

    std::cout << "Starting full optimization over " << historical_candles.size() << " candles..." << std::endl;

    for (int short_p = 5; short_p <= 15; short_p += 2) {
        for (int long_p = 20; long_p <= 50; long_p += 5) {
            if (short_p >= long_p) continue;
            for (int rsi_p = 7; rsi_p <= 21; rsi_p += 2) {
                // For simplicity, we keep ATR period fixed for now, but it could be added here
                StrategyParams current_params = {short_p, long_p, rsi_p, 14}; // ATR period fixed at 14
                current_params.performance = simulateBacktest(historical_candles, current_params);

                if (current_params.performance > best_params.performance) {
                    best_params = current_params;
                    // Optional: Log progress
                    // std::cout << "New best: SMA " << best_params.sma_short << "/" << best_params.sma_long
                    //           << " RSI " << best_params.rsi_period << " Perf: " << best_params.performance << std::endl;
                }
            }
        }
    }
    return best_params;
}
