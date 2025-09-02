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

struct PairConfig {
    std::string ticker;
    std::string interval;
    int lookback; // Now used as min_candles
};

struct Candle {
    std::string datetime;
    double open, high, low, close;
};

// Holds the parameters for our strategy
struct StrategyParams {
    int sma_short;
    int sma_long;
    double performance = -1e9; // Used to track the profitability of these params
};

// (Functions readConfig, readData, computeTrueRange, computeATR, computeRSI remain the same as signalv_6.cpp)
// --- Paste them here ---
std::vector<PairConfig> readConfig(const std::string& file);
std::vector<Candle> readData(const std::string& file);
double computeTrueRange(const Candle& current, const Candle& previous);
double computeATR(const std::vector<Candle>& candles, int period);
double computeRSI(const std::vector<double>& closes, int period);
// --- Assume they are present ---

double computeSMA(const std::vector<double>& prices, size_t start_index, int period) {
    if (start_index < period - 1) return 0;
    double sum = 0.0;
    for (size_t i = start_index - period + 1; i <= start_index; ++i) {
        sum += prices[i];
    }
    return sum / period;
}

// ** SIMULATOR FUNCTION **
// This function simulates our trading strategy for a given set of parameters
// over a slice of historical data and returns a performance score (e.g., profit).
double simulateBacktest(const std::vector<double>& closes, const StrategyParams& params) {
    if (closes.size() < params.sma_long) return -1e9; // Not enough data

    double profit = 0.0;
    bool in_position = false;
    double entry_price = 0.0;

    for (size_t i = params.sma_long; i < closes.size(); ++i) {
        double sma_short = computeSMA(closes, i, params.sma_short);
        double sma_long = computeSMA(closes, i, params.sma_long);

        // Simple crossover logic for the backtest
        if (sma_short > sma_long && !in_position) { // Buy signal
            in_position = true;
            entry_price = closes[i];
        } else if (sma_short < sma_long && in_position) { // Sell signal
            in_position = false;
            profit += (closes[i] - entry_price);
        }
    }
    return profit;
}

// ** OPTIMIZER FUNCTION **
// This function loops through different parameters to find the best combination.
StrategyParams findBestParameters(const std::vector<double>& historical_closes) {
    StrategyParams best_params;

    std::cout << "Starting optimization over " << historical_closes.size() << " candles..." << std::endl;

    // --- Define the range of parameters to test ---
    for (int short_p = 5; short_p <= 15; ++short_p) {
        for (int long_p = 20; long_p <= 50; long_p += 5) {
            if (short_p >= long_p) continue;

            StrategyParams current_params = {short_p, long_p};
            current_params.performance = simulateBacktest(historical_closes, current_params);

            if (current_params.performance > best_params.performance) {
                best_params = current_params;
                std::cout << "New best params found: SMA " << best_params.sma_short << "/" << best_params.sma_long
                << " | Performance: " << best_params.performance << std::endl;
            }
        }
    }
    return best_params;
}


int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    std::vector<PairConfig> cfgs = readConfig(argv[1]);
    if (cfgs.empty()) {
        std::cerr << "No tickers found in config file." << std::endl;
        return 1;
    }

    for (const auto& cfg : cfgs) {
        std::cout << "\n======================================================\n"
        << "Processing: " << cfg.ticker << " (" << cfg.interval << ")"
        << "\n======================================================\n";

        std::vector<Candle> candles = readData(cfg.ticker + ".csv");

    if (candles.size() < 100) { // Need a good amount of data for optimization
        std::cerr << "Not enough data for optimization. Need at least 100 candles." << std::endl;
        return 1;
    }

    std::vector<double> closes;
    for(const auto& c : candles) closes.push_back(c.close);

    // --- Adaptive Logic Step 1: Define the optimization window ---
    // We'll use all but the most recent candle for optimization.
    std::vector<double> optimization_closes(closes.begin(), closes.end() - 1);

    // --- Adaptive Logic Step 2: Find the best parameters ---
    StrategyParams optimal_params = findBestParameters(optimization_closes);
    std::cout << "\n--- Optimization Complete ---\n"
    << "Optimal parameters found: SMA " << optimal_params.sma_short << "/" << optimal_params.sma_long << "\n\n";

    // --- Adaptive Logic Step 3: Generate signal using the best parameters ---
    // Now we use the full dataset to calculate indicators for the *current* candle
    double sma_short = computeSMA(closes, closes.size() - 1, optimal_params.sma_short);
    double sma_long = computeSMA(closes, closes.size() - 1, optimal_params.sma_long);
    double atr14 = computeATR(candles, 14);
    double rsi14 = computeRSI(closes, 14);

    std::string signal = "HOLD";
    double entry = candles.back().close;
    double sl = 0, tp = 0;

    if(sma_short > sma_long && rsi14 > 50 && sma_short > 0) {
        signal = "BUY";
        sl = entry - 1.5 * atr14;
        tp = entry + 2.0 * atr14;
    } else if(sma_short < sma_long && rsi14 < 50 && sma_short > 0) {
        signal = "SELL";
        sl = entry + 1.5 * atr14;
        tp = entry - 2.0 * atr14;
    }

    std::cout << "--- FINAL SIGNAL ---\n";
    std::cout << candles.back().datetime << " | " << cfg.ticker << " | " << signal;
    if(signal != "HOLD")
        std::cout << " | Entry=" << entry << " SL=" << sl << " TP=" << tp;
    std::cout << " | Using SMA" << optimal_params.sma_short << "=" << sma_short
    << " SMA" << optimal_params.sma_long << "=" << sma_long
    << " RSI14=" << rsi14 << "\n";
    }
    return 0;
}


// You would need to paste the full function definitions for readConfig, readData,
// computeTrueRange, computeATR, and computeRSI from the previous version for this to compile.

double computeTrueRange(const Candle& current, const Candle& previous) {
    double tr1 = current.high - current.low;
    double tr2 = std::abs(current.high - previous.close);
    double tr3 = std::abs(current.low - previous.close);
    return std::max({tr1, tr2, tr3});
}

double computeATR(const std::vector<Candle>& candles, int period) {
    if (candles.size() < period + 1) return 0;
    std::vector<double> trueRanges;
    for (size_t i = candles.size() - period; i < candles.size(); ++i) {
        trueRanges.push_back(computeTrueRange(candles[i], candles[i-1]));
    }
    double sum_tr = std::accumulate(trueRanges.begin(), trueRanges.end(), 0.0);
    return sum_tr / period;
}

// Function to compute Relative Strength Index (RSI)
double computeRSI(const std::vector<double>& closes, int period) {
    if (closes.size() < period + 1) return 0;

    std::vector<double> gains;
    std::vector<double> losses;

    for (size_t i = closes.size() - period; i < closes.size(); ++i) {
        double change = closes[i] - closes[i-1];
        if (change > 0) {
            gains.push_back(change);
            losses.push_back(0);
        } else {
            gains.push_back(0);
            losses.push_back(-change);
        }
    }

    double avg_gain = std::accumulate(gains.begin(), gains.end(), 0.0) / period;
    double avg_loss = std::accumulate(losses.begin(), losses.end(), 0.0) / period;

    if (avg_loss == 0) return 100.0; // Avoid division by zero

    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
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

