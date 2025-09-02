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
    int lookback;
};

struct Candle {
    std::string datetime;
    double open, high, low, close;
};

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
    // Skip header
    getline(f, line);
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
            candles.push_back({
                datetime,
                std::stod(open),
                              std::stod(high),
                              std::stod(low),
                              std::stod(close)
            });
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << " -> " << e.what() << std::endl;
        }
    }
    return candles;
}

double computeSMA(const std::vector<double>& prices, int period) {
    if(prices.size() < period) return 0;
    double sum = 0;
    for(int i = prices.size() - period; i < prices.size(); ++i)
        sum += prices[i];
    return sum / period;
}

// Function to compute True Range
double computeTrueRange(const Candle& current, const Candle& previous) {
    double tr1 = current.high - current.low;
    double tr2 = std::abs(current.high - previous.close);
    double tr3 = std::abs(current.low - previous.close);
    return std::max({tr1, tr2, tr3});
}

// Function to compute Average True Range (ATR)
double computeATR(const std::vector<Candle>& candles, int period) {
    if (candles.size() < period + 1) return 0; // Need at least period + 1 candles to compute ATR
    std::vector<double> trueRanges;
    for (size_t i = candles.size() - period; i < candles.size(); ++i) {
        trueRanges.push_back(computeTrueRange(candles[i], candles[i-1]));
    }
    double sum_tr = std::accumulate(trueRanges.begin(), trueRanges.end(), 0.0);
    return sum_tr / period;
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    std::vector<PairConfig> cfgs = readConfig(argv[1]);
    if(cfgs.empty()) {
        std::cerr << "No tickers found in config file." << std::endl;
        return 1;
    }

    // Process first ticker
    const auto& cfg = cfgs[0];
    std::string csv_file = cfg.ticker + ".csv";
    std::vector<Candle> candles = readData(csv_file);

    if (candles.size() < 20) {
        std::cerr << "Not enough data to run strategy. Need at least 20 candles." << std::endl;
        return 1;
    }

    std::vector<double> closes;
    for(const auto& candle : candles) {
        closes.push_back(candle.close);
    }

    // Compute SMA and ATR
    double sma5 = computeSMA(closes, 5);
    double sma20 = computeSMA(closes, 20);
    double atr = computeATR(candles, 14); // Common ATR period is 14

    std::string signal = "HOLD";
    double entry = candles.back().close;
    double sl = 0, tp = 0;

    // SMA Crossover Strategy with ATR-based Stop-Loss and Take-Profit
    if(sma5 > sma20 && sma5 > 0 && sma20 > 0) {
        signal = "BUY";
        // Set stop-loss and take-profit based on a multiple of ATR
        sl = entry - 1.5 * atr; // 1.5x ATR below entry
        tp = entry + 2.0 * atr; // 2.0x ATR above entry
    } else if(sma5 < sma20 && sma5 > 0 && sma20 > 0) {
        signal = "SELL";
        // Set stop-loss and take-profit based on a multiple of ATR
        sl = entry + 1.5 * atr; // 1.5x ATR above entry
        tp = entry - 2.0 * atr; // 2.0x ATR below entry
    }

    // Output signal
    std::cout << candles.back().datetime << " | " << cfg.ticker << " | " << signal;
    if(signal != "HOLD")
        std::cout << " | Entry=" << entry << " SL=" << sl << " TP=" << tp;
    std::cout << " | SMA5=" << sma5 << " SMA20=" << sma20 << " ATR=" << atr << "\n";

    return 0;
}
