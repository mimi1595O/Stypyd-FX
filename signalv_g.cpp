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
#include <random>
#include <thread>

// --- Structs ---
struct PairConfig { std::string ticker; std::string interval;};
struct Candle { std::string datetime; double open; double high; double low; double close; long long volume; double atr; };
struct StrategyParams { int sma_short = 5; int sma_long = 20; int rsi_period = 14; double performance = -1e9; };

// --- Forward Declarations for clarity ---
std::vector<PairConfig> readConfig(const std::string& file);
std::vector<Candle> readData(const std::string& file);
bool hasVolumeData(const std::vector<Candle>& candles);
void logTrade(const std::string& datetime, const std::string& ticker, const std::string& signal, double entry, double sl, double tp);
double computeSMA(const std::vector<double>& prices, size_t end_index, int period);
double computeRSI(const std::vector<double>& closes, size_t end_index, int period);
int computeOBVDirection(const std::vector<Candle>& candles, int period);
double simulateBacktest(const std::vector<Candle>& candles, const StrategyParams& params);
StrategyParams findBestParameters_Random(const std::vector<Candle>& historical_candles, int num_iterations);
void process_ticker(const PairConfig& cfg);



// --- Optimization ---


// --- Core Task for a Thread ---
void process_ticker(const PairConfig& cfg) {
    std::stringstream output_stream;
    output_stream << "\n--- Processing " << cfg.ticker << " ---" << std::endl;
    auto candles = readData(cfg.ticker + ".csv");

    if (candles.size() < 1) {
        output_stream << "Not enough data for " << cfg.ticker << ". Skipping." << std::endl;
        std::cout << output_stream.str();
        return;
    }

    std::vector<Candle> optimization_candles(candles.begin(), candles.end() - 1);
    StrategyParams optimal_params = findBestParameters_Random(optimization_candles, 100);

    output_stream << "Optimal Params for " << cfg.ticker << ": SMA(" << optimal_params.sma_short << "/" << optimal_params.sma_long
    << "), RSI(" << optimal_params.rsi_period << ")\n";

    std::vector<double> closes;
    for (const auto& c : candles) closes.push_back(c.close);

    double current_atr = candles.back().atr;
    double entry = candles.back().close;
    const float MINIMUM_ATR_PERCENT = 0.10;
    const float high_volatility = 0.30;
    const float extreme_volatility = 0.50;
    float current_atr_percent = (int)((current_atr / entry) * 100*1000);
    current_atr_percent = current_atr_percent/1000;
    bool is_volatile_enough =  current_atr_percent > MINIMUM_ATR_PERCENT;

    double sma_short = computeSMA(closes, closes.size() - 1, optimal_params.sma_short);
    double sma_long = computeSMA(closes, closes.size() - 1, optimal_params.sma_long);
    double rsi = computeRSI(closes, closes.size() - 1, optimal_params.rsi_period);

    bool use_volume = hasVolumeData(candles);
    int obv_direction = use_volume ? computeOBVDirection(candles, 14) : 0;

    std::string signal = "HOLD";
    if (use_volume) {
        if (sma_short > sma_long && rsi > 50 && obv_direction == 1 && rsi < 70 ) signal = "BUY";
        else if (sma_short < sma_long && rsi < 50 && obv_direction == -1 && rsi > 30 ) signal = "SELL";
    } else {
        if (sma_short > sma_long && rsi > 50 && rsi < 70 ) signal = "BUY";
        else if (sma_short < sma_long && rsi < 50 && rsi > 30 ) signal = "SELL";
    }

    output_stream << "FINAL SIGNAL: " << candles.back().datetime << " | " << cfg.ticker << " | ";

    if (signal != "HOLD" && is_volatile_enough) {
        double sl = (signal == "BUY") ? entry - 1.5 * current_atr : entry + 1.5 * current_atr;
        double tp = (signal == "BUY") ? entry + 2.0 * current_atr : entry - 2.0 * current_atr;
        output_stream << signal << " | Entry=" << entry << " SL=" << sl << " TP=" << tp;
        logTrade(candles.back().datetime, cfg.ticker, signal, entry, sl, tp);
    } else {
        std::string reason = (signal != "HOLD" && !is_volatile_enough) ? " (Ignored: Low Volatility)" : "";
        output_stream << "HOLD" << reason;
    }

    if (use_volume) output_stream << " | OBV Dir=" << obv_direction;
    output_stream << "| ATR% = " << current_atr_percent;
    if(current_atr_percent > high_volatility && current_atr_percent < extreme_volatility) output_stream << "WARNING! HIGH VOLATILITY (> 0.30)";
    if(current_atr_percent > extreme_volatility) output_stream << "WARNING EXTREMELY HIGH VOLATILITY (> 0.50)";
    output_stream << std::endl;

    std::cout << output_stream.str();
}

// --- Main Program ---
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    auto cfgs = readConfig(argv[1]);
    std::vector<std::thread> workers;

    for (const auto& cfg : cfgs) {
        // Corrected typo from ProcessTicker to process_ticker
        workers.emplace_back(process_ticker, cfg);
    }

    std::cout << "Launched " << workers.size() << " worker threads. Waiting for completion..." << std::endl;
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    std::cout << "\n--- All tasks complete. ---" << std::endl;
    return 0;
}

// --- Full Function Implementations ---
std::vector<PairConfig> readConfig(const std::string& file) {
    std::vector<PairConfig> cfgs;
    std::ifstream f(file);
    std::string line;
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        PairConfig p;
        ss >> p.ticker >> p.interval; // Reads only ticker and interval
        cfgs.push_back(p);
    }
    return cfgs;
}
StrategyParams findBestParameters_Random(const std::vector<Candle>& historical_candles, int num_iterations) {
    StrategyParams best_params;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib_short(5, 15);
    std::uniform_int_distribution<> distrib_long_diff(5, 30);
    std::uniform_int_distribution<> distrib_rsi(7, 21);

    for (int i = 0; i < num_iterations; ++i) {
        StrategyParams current_params;
        current_params.sma_short = distrib_short(gen);
        current_params.sma_long = current_params.sma_short + distrib_long_diff(gen);
        current_params.rsi_period = distrib_rsi(gen);
        current_params.performance = simulateBacktest(historical_candles, current_params);

        if (current_params.performance > best_params.performance) {
            best_params = current_params;
        }
    }
    return best_params;
}


std::vector<Candle> readData(const std::string& file) {
    std::vector<Candle> candles;
    std::ifstream f(file);
    std::string line;
    getline(f, line); // Skip header
    while (getline(f, line)) {
        std::stringstream ss(line);
        std::string dt, o, h, l, c, ac, v, atr_str;
        getline(ss, dt, ',');
        getline(ss, o, ',');
        getline(ss, h, ',');
        getline(ss, l, ',');
        getline(ss, c, ',');
        getline(ss, ac, ',');
        getline(ss, v, ',');
        getline(ss, atr_str, ',');
        if (o.empty() || c.empty()) continue;
        try {
            candles.push_back({dt, std::stod(o), std::stod(h), std::stod(l), std::stod(c), std::stoll(v), std::stod(atr_str)});
        } catch (const std::exception& e) {}
    }
    return candles;
}
bool hasVolumeData(const std::vector<Candle>& candles) {
    long long total_volume = 0;
    for(const auto& c : candles) total_volume += c.volume;
    return total_volume > 0;
}
void logTrade(const std::string& datetime, const std::string& ticker, const std::string& signal, double entry, double sl, double tp) {
    std::ofstream logfile("tradelog.csv", std::ios::app);
    if (logfile.tellp() == 0) logfile << "Datetime,Ticker,Signal,Entry,StopLoss,TakeProfit\n";
    logfile << datetime << "," << ticker << "," << signal << "," << std::fixed << std::setprecision(5) << entry << "," << sl << "," << tp << "\n";
}
double computeSMA(const std::vector<double>& prices, size_t end_index, int period) {
    if (end_index + 1 < period || period <= 0) return 0;
    double sum = std::accumulate(prices.begin() + end_index - period + 1, prices.begin() + end_index + 1, 0.0);
    return sum / period;
}
double computeRSI(const std::vector<double>& closes, size_t end_index, int period) {
    if (end_index + 1 < period + 1) return 0;
    double gain = 0.0, loss = 0.0;
    for (size_t i = end_index - period + 1; i <= end_index; ++i) {
        double change = closes[i] - closes[i-1];
        if (change > 0) gain += change; else loss -= change;
    }
    if (loss == 0) return 100.0;
    double rs = (gain / period) / (loss / period);
    return 100.0 - (100.0 / (1.0 + rs));
}
int computeOBVDirection(const std::vector<Candle>& candles, int period) {
    if (candles.size() < period + 1) return 0;
    long long current_obv = 0;
    std::vector<long long> history;
    for (size_t i = candles.size() - period; i < candles.size(); ++i) {
        if (candles[i].close > candles[i-1].close) current_obv += candles[i].volume;
        else if (candles[i].close < candles[i-1].close) current_obv -= candles[i].volume;
        history.push_back(current_obv);
    }
    if (history.back() > history.front()) return 1;
    if (history.back() < history.front()) return -1;
    return 0;
}
double simulateBacktest(const std::vector<Candle>& candles, const StrategyParams& params) {
    if (candles.size() < std::max(params.sma_long, params.rsi_period) + 1) return -1e9;
    std::vector<double> closes;
    for(const auto& c : candles) closes.push_back(c.close);
    double profit = 0.0;
    bool in_pos = false;
    double entry = 0.0;
    size_t start = std::max(params.sma_long, params.rsi_period) + 1;
    for (size_t i = start; i < candles.size(); ++i) {
        double s_sma = computeSMA(closes, i, params.sma_short);
        double l_sma = computeSMA(closes, i, params.sma_long);
        double rsi = computeRSI(closes, i, params.rsi_period);
        if (!in_pos && s_sma > l_sma && rsi > 50) { in_pos = true; entry = closes[i]; }
        else if (in_pos && s_sma < l_sma) { profit += (closes[i] - entry); in_pos = false; }
    }
    return profit;
}
