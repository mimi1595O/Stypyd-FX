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
    // 'lookback' parameter removed as it is no longer used.
};

struct Candle {
    std::string datetime;
    double open, high, low, close;
    long long volume; // Add volume to the candle structure
};

// --- Helper Functions ---

std::vector<PairConfig> readConfig(const std::string& file) {
    std::vector<PairConfig> cfgs;
    std::ifstream f(file);
    std::string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        PairConfig p;
        // Updated to read only two parameters from conf.txt
        ss >> p.ticker >> p.interval;
        cfgs.push_back(p);
    }
    return cfgs;
}




void logTrade(const std::string& datetime, const std::string& ticker, const std::string& signal, double entry, double sl, double tp) {
    // Open the file in append mode. std::ios::app ensures we add to the end of the file.
    std::ofstream logfile("tradelog.csv", std::ios::app);

    // If the file is empty, write the header first.
    if (logfile.tellp() == 0) {
        logfile << "Datetime,Ticker,Signal,Entry,StopLoss,TakeProfit\n";
    }

    logfile << datetime << ","
    << ticker << ","
    << signal << ","
    << std::fixed << std::setprecision(5) << entry << ","
    << sl << ","
    << tp << "\n";
}

// ** UPDATED: readData now parses volume **
std::vector<Candle> readData(const std::string& file) {
    std::vector<Candle> candles;
    std::ifstream f(file);
    std::string line;
    getline(f, line); // Skip header
    while (getline(f, line)) {
        std::stringstream ss(line);
        std::string dt, o, h, l, c, ac, v;
        getline(ss, dt, ',');
        getline(ss, o, ',');
        getline(ss, h, ',');
        getline(ss, l, ',');
        getline(ss, c, ',');
        getline(ss, ac, ',');
        getline(ss, v, ',');
        if (o.empty() || c.empty()) continue;
        try {
            // Convert volume to long long, handle potential empty string or parse errors
            long long vol = v.empty() ? 0 : std::stoll(v);
            candles.push_back({dt, std::stod(o), std::stod(h), std::stod(l), std::stod(c), vol});
        } catch (const std::exception& e) {
            // If there's a parsing error, assume volume is 0
            candles.push_back({dt, std::stod(o), std::stod(h), std::stod(l), std::stod(c), 0});
        }
    }
    return candles;
}

// ** NEW: Function to check for meaningful volume data **
bool hasVolumeData(const std::vector<Candle>& candles) {
    if (candles.size() < 2) return false;
    long long total_volume = 0;
    // Sum volume over the last N candles to check for activity
    size_t check_range = std::min((size_t)50, candles.size());
    for(size_t i = candles.size() - check_range; i < candles.size(); ++i) {
        total_volume += candles[i].volume;
    }
    // If the total volume is greater than a small threshold, we have volume data
    return total_volume > 0;
}


// adaptive algorithm
struct StrategyParams {
    int sma_short = 5;
    int sma_long = 20;
    int rsi_period = 14;
    int atr_period = 14;
    double performance = -1e9; // Performance score
};






// --- Indicator Functions ---

double computeSMA(const std::vector<double>& prices, int period) {
    if (prices.size() < period) return 0;
    double sum = std::accumulate(prices.end() - period, prices.end(), 0.0);
    return sum / period;
}

double computeRSI(const std::vector<double>& closes, int period) {
    if (closes.size() < period + 1) return 0;
    double gain = 0.0, loss = 0.0;
    for (size_t i = closes.size() - period; i < closes.size(); ++i) {
        double change = closes[i] - closes[i - 1];
        if (change > 0) gain += change;
        else loss += -change;
    }
    double avg_gain = gain / period;
    double avg_loss = loss / period;
    if (avg_loss == 0) return 100.0;
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

// ** NEW: On-Balance Volume (OBV) Indicator **
// Returns the direction of OBV (1 for up, -1 for down) over the last few periods.
int computeOBVDirection(const std::vector<Candle>& candles, int period) {
    if (candles.size() < period + 1) return 0;

    long long current_obv = 0;
    // We need a small history of OBV to see its trend
    std::vector<long long> obv_history;

    // Calculate OBV for the last `period` candles
    for (size_t i = candles.size() - period; i < candles.size(); ++i) {
        if (candles[i].close > candles[i-1].close) {
            current_obv += candles[i].volume;
        } else if (candles[i].close < candles[i-1].close) {
            current_obv -= candles[i].volume;
        }
        obv_history.push_back(current_obv);
    }

    // Check if the OBV is trending up or down in the last few periods
    if (obv_history.back() > obv_history.front()) {
        return 1; // OBV is rising
    } else if (obv_history.back() < obv_history.front()) {
        return -1; // OBV is falling
    }
    return 0; // OBV is flat
}

double computeTrueRange(const Candle& current, const Candle& previous) {
    double tr1 = current.high - current.low;
    double tr2 = std::abs(current.high - previous.close);
    double tr3 = std::abs(current.low - previous.close);
    return std::max({tr1, tr2, tr3});
}

double computeATR(const std::vector<Candle>& candles, int period) {
    if (candles.size() < period + 1) return 0;
    double sum_tr = 0;
    for (size_t i = candles.size() - period; i < candles.size(); ++i) {
        sum_tr += computeTrueRange(candles[i], candles[i-1]);
    }
    return sum_tr / period;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    auto cfgs = readConfig(argv[1]);

    for (const auto& cfg : cfgs) {
        std::cout << "\n--- Processing " << cfg.ticker << " ---" << std::endl;
        auto candles = readData(cfg.ticker + ".csv");

        if (candles.size() < 100) { // Need at least 21 for 20-period SMA and prior candle
            std::cerr << "Not enough data for optimization. Need at least 100. Found: " << candles.size() << std::endl;
            continue;
        }



        std::vector<double> closes;
        for(const auto& c : candles) closes.push_back(c.close);

        // --- ADAPTIVE LOGIC ---
        bool use_volume = hasVolumeData(candles);

        // --- Calculate Indicators ---
//         std::vector<Candle> optimization_candles(candles.begin(), candles.end() - 1);
//         StrategyParams optimal_params = findBestParameters(optimization_candles);
//
//         std::cout << "\n--- Optimization Complete for " << cfg.ticker << " ---\n"
//         << "Optimal Params Found: SMA(" << optimal_params.sma_short << "/" << optimal_params.sma_long
//         << "), RSI(" << optimal_params.rsi_period << "), ATR(" << optimal_params.atr_period << ")\n\n";
//
//         std::vector<double> closes;
//         for (const auto& c : candles) closes.push_back(c.close);
//
//         // --- Generate final signal using ALL optimal parameters ---
//         double sma_short = computeSMA(closes, closes.size() - 1, optimal_params.sma_short);
//         double sma_long = computeSMA(closes, closes.size() - 1, optimal_params.sma_long);
//         double rsi = computeRSI(closes, closes.size() - 1, optimal_params.rsi_period);
//         double atr = computeATR(candles, candles.size() - 1, optimal_params.atr_period);
//
//         std::string signal = "HOLD";
//         double entry = candles.back().close;
//         double sl = 0, tp = 0;
        int obv_direction = 0;

        if (use_volume) {
            obv_direction = computeOBVDirection(candles, 5); // Check OBV trend over 5 periods
            std::cout << "Volume data detected. Activating OBV indicator." << std::endl;
        } else {
            std::cout << "No volume data detected. Using price-only indicators." << std::endl;
        }

        std::string signal = "HOLD";
        double entry = candles.back().close;
        double sl = 0, tp = 0;
        double atr14 = computeATR(candles, 14);

        // --- STRATEGY LOGIC ---
        if (use_volume) {
            // **Strategy with Volume Confirmation**
            if (sma_short > sma_long && rsi14 > 50 && obv_direction == 1) {
                signal = "BUY";
            } else if (sma_short < sma_long && rsi14 < 50 && obv_direction == -1) {
                signal = "SELL";
            }
        } else {
            // **Fallback Strategy (No Volume)**
            if (sma_short > sma_long && rsi14 > 50) {
                signal = "BUY";
            } else if (sma_short < sma_long && rsi14 < 50) {
                signal = "SELL";
            }
        }

        if (signal != "HOLD") {
            sl = (signal == "BUY") ? entry - 1.5 * atr14 : entry + 1.5 * atr14;
            tp = (signal == "BUY") ? entry + 2.0 * atr14 : entry - 2.0 * atr14;
        }

        // --- Output logic ---
        std::cout << "FINAL SIGNAL: " << candles.back().datetime << " | " << cfg.ticker << " | " << signal;
        if (signal != "HOLD") std::cout << " | Entry=" << entry << " SL=" << sl << " TP=" << tp;
        if(use_volume) std::cout << " | OBV Dir=" << obv_direction;
        std::cout << std::endl;

        //add logfile logic here
        logTrade(candles.back().datetime, cfg.ticker, signal, entry, sl, tp);


    }

    return 0;
}

