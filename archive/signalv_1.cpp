// usdjpy_signal.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

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

int main() {
    std::ifstream file("usdjpy.csv");
    if(!file.is_open()) { std::cerr << "Cannot open CSV\n"; return 1; }

    std::string line;
    std::vector<Candle> candles;
    getline(file, line); // skip header

    while(getline(file, line)) {
        std::stringstream ss(line);
        std::string datetime, open, high, low, close, adjclose, volume;
        getline(ss, datetime, ',');
        getline(ss, open, ',');
        getline(ss, high, ',');
        getline(ss, low, ',');
        getline(ss, close, ',');
        getline(ss, adjclose, ',');
        getline(ss, volume, ',');

        candles.push_back({datetime, std::stod(close)});
    }

    std::vector<double> closes;
    for(auto &c : candles) closes.push_back(c.close);

    double sma5 = computeSMA(closes, 5);
    double sma20 = computeSMA(closes, 20);

    std::string signal = "HOLD";
    if(sma5 > sma20) signal = "BUY";
    else if(sma5 < sma20) signal = "SELL";

    std::cout << candles.back().datetime << " | USD/JPY | " << signal 
              << " | SMA5=" << sma5 << " SMA20=" << sma20 << "\n";

    return 0;
}
