#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>

struct Candle {
    std::string datetime;
    double close;
};

double computeSMA(const std::vector<double>& prices, int period) {
    if(prices.size() < period) return 0;
    double sum = 0;
    for(int i = prices.size() - period; i < prices.size(); ++i)
        sum += prices[i];
    return sum / period;
}

bool isNumber(const std::string& s) {
    try {
        std::stod(s);
        return true;
    } catch (...) {
        return false;
    }
}


double computeStdDev(const std::vector<double>& prices, int n) {
    if(prices.size() < n) return 0;
    double mean = 0;
    for(int i = prices.size()-n; i < prices.size(); ++i) mean += prices[i];
    mean /= n;
    double sumSq = 0;
    for(int i = prices.size()-n; i < prices.size(); ++i)
        sumSq += (prices[i]-mean)*(prices[i]-mean);
    return std::sqrt(sumSq/n);
}

int main() {
    std::ifstream file("usdjpy.csv");
    if(!file.is_open()) { std::cerr << "Cannot open CSV\n"; return 1; }

    std::string line;
    std::vector<Candle> candles;
    getline(file, line); // skip header

    while(getline(file, line)) {
        if(line.empty()) continue; // skip empty lines
        std::stringstream ss(line);
        std::string datetime, open, high, low, close, adjclose, volume;
        getline(ss, datetime, ',');
        getline(ss, open, ',');
        getline(ss, high, ',');
        getline(ss, low, ',');
        getline(ss, close, ',');
        getline(ss, adjclose, ',');
        getline(ss, volume, ',');

        if(!isNumber(close)) continue; // skip bad lines

        candles.push_back({datetime, std::stod(close)});
    }

    if(candles.size() < 20) {
        std::cerr << "Not enough data for SMA calculation\n";
        return 1;
    }

    std::vector<double> closes;
    for(auto &c : candles) closes.push_back(c.close);

    double sma5 = computeSMA(closes, 5);
    double sma20 = computeSMA(closes, 20);

    // <<< Insert adaptive sideways detection here >>>
    int lookback = 20;
    double stddev = computeStdDev(closes, lookback);
    double smaDiff = std::abs(sma5 - sma20);
    double threshold = stddev; // adaptive

    std::string signal = "HOLD";
    double entry = closes.back();
    double sl = 0, tp = 0;

    if(smaDiff >= threshold) { // only trade if SMA difference > recent volatility
        if(sma5 > sma20) {
            signal = "BUY";
            sl = entry * 0.998;
            tp = entry * 1.004;
        } else if(sma5 < sma20) {
            signal = "SELL";
            sl = entry * 1.002;
            tp = entry * 0.996;
        }
    }

    // Output signal
    std::cout << candles.back().datetime << " | USD/JPY | " << signal;
    if(signal != "HOLD")
        std::cout << " | Entry=" << entry << " SL=" << sl << " TP=" << tp;
    std::cout << " | SMA5=" << sma5 << " SMA20=" << sma20 << " StdDev=" << stddev << "\n";
}
