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
struct PairConfig {
    std::string ticker;
    std::string interval;
};

struct Candle {
    std::string datetime;
    double open, high, low, close;
    long long volume;
};

struct StrategyParams {
    int sma_short = 5;
    int sma_long = 20;
    int rsi_period = 14;
    int atr_period = 14;
    double performance = -1e9;
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
        ss >> p.ticker >> p.interval; // Reads only ticker and interval
        cfgs.push_back(p);
    }
    return cfgs;
}

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
            long long vol = v.empty() ? 0 : std::stoll(v);
            candles.push_back({dt, std::stod(o), std::stod(h), std::stod(l), std::stod(c), vol});
        } catch (const std::exception& e) {
            candles.push_back({dt, std::stod(o), std::stod(h), std::stod(l), std::stod(c), 0});
        }
    }
    return candles;
}

bool hasVolumeData(const std::vector<Candle>& candles) {
    if (candles.empty()) return false;
    long long total_volume = 0;
    for(const auto& c : candles) {
        total_volume += c.volume;
    }
    return total_volume > 0;
}

void logTrade(const std::string& datetime, const std::string& ticker, const std::string& signal, double entry, double sl, double tp) {
    std::ofstream logfile("tradelog.csv", std::ios::app);
    if (logfile.tellp() == 0) {
        logfile << "Datetime,Ticker,Signal,Entry,StopLoss,TakeProfit\n";
    }
    logfile << datetime << "," << ticker << "," << signal << ","
    << std::fixed << std::setprecision(5) << entry << "," << sl << "," << tp << "\n";
}


// --- Indicator Functions ---

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
    double gain_sum = 0.0, loss_sum = 0.0;
    for (size_t i = end_index - period + 1; i <= end_index; ++i) {
        double change = closes[i] - closes[i-1];
        if (change > 0) gain_sum += change;
        else loss_sum += -change;
    }
    double avg_gain = gain_sum / period;
    double avg_loss = loss_sum / period;
    if (avg_loss == 0) return 100.0;
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

int computeOBVDirection(const std::vector<Candle>& candles, int period) {
    if (candles.size() < period + 1) return 0;
    long long current_obv = 0;
    std::vector<long long> obv_history;
    for (size_t i = candles.size() - period; i < candles.size(); ++i) {
        if (candles[i].close > candles[i-1].close) current_obv += candles[i].volume;
        else if (candles[i].close < candles[i-1].close) current_obv -= candles[i].volume;
        obv_history.push_back(current_obv);
    }
    if (obv_history.back() > obv_history.front()) return 1;
    if (obv_history.back() < obv_history.front()) return -1;
    return 0;
}

double computeTrueRange(const Candle& current, const Candle& previous) {
    return std::max({current.high - current.low, std::abs(current.high - previous.close), std::abs(current.low - previous.close)});
}

double computeATR(const std::vector<Candle>& candles, size_t end_index, int period) {
    if (end_index + 1 < period + 1) return 0;
    double tr_sum = 0.0;
    for (size_t i = end_index - period + 1; i <= end_index; ++i) {
        tr_sum += computeTrueRange(candles[i], candles[i-1]);
    }
    return tr_sum / period;
}


// --- Optimization Functions ---

double simulateBacktest(const std::vector<Candle>& candles, const StrategyParams& params) {
    if (candles.size() < std::max(params.sma_long, params.rsi_period) + 1) return -1e9;
    std::vector<double> closes;
    for(const auto& c : candles) closes.push_back(c.close);

    double profit = 0.0;
    bool in_position = false;
    double entry_price = 0.0;
    size_t start_index = std::max(params.sma_long, params.rsi_period) + 1;

    for (size_t i = start_index; i < candles.size(); ++i) {
        double sma_short = computeSMA(closes, i, params.sma_short);
        double sma_long = computeSMA(closes, i, params.sma_long);
        double rsi = computeRSI(closes, i, params.rsi_period);

        if (!in_position && sma_short > sma_long && rsi > 50) {
            in_position = true;
            entry_price = closes[i];
        } else if (in_position && sma_short < sma_long) {
            profit += (closes[i] - entry_price);
            in_position = false;
        }
    }
    return profit;
}

StrategyParams findBestParametersRandom(const std::vector<Candle>& historical_candles) {
    StrategyParams best_params;
    int short_p, long_p, rsi_p;
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> short_p_rand(5, 30);
    std::uniform_int_distribution<> short_p_diff_rand(3, 40);
    std::uniform_int_distribution<> rsi_p_rand(7, 21);

    for(int i = 0; i < 100; i++){
        short_p = short_p_rand(gen);
        long_p = short_p_diff_rand(gen)+short_p;
        rsi_p = rsi_p_rand(gen);
        StrategyParams current_params = {short_p, long_p, rsi_p, 14};
        current_params.performance = simulateBacktest(historical_candles, current_params);
        if (current_params.performance > best_params.performance)
        {
            best_params = current_params;

        }





    }
    return best_params;
}


void ProcessTicker(const PairConfig& cfg){
    std::stringstream output_stream; // Build the output in a local string to avoid jumbling
    output_stream << "\n--- Processing " << cfg.ticker << " ---" << std::endl;
    auto candles = readData(cfg.ticker + ".csv");

    if (candles.size() < 100) {
        std::cerr << "Not enough data for optimization. Need at least 100. Found: " << candles.size() << std::endl;
        return;
    }

    // --- Adaptive Optimization ---
    std::vector<Candle> optimization_candles(candles.begin(), candles.end() - 1);
    output_stream << "Starting full optimization over " << optimization_candles.size() << " candles..." << std::endl;
    StrategyParams optimal_params = findBestParametersRandom(optimization_candles);

    output_stream << "\n--- Optimization Complete for " << cfg.ticker << " ---\n"
    << "Optimal Params Found: SMA(" << optimal_params.sma_short << "/" << optimal_params.sma_long
    << "), RSI(" << optimal_params.rsi_period << ")\n\n";

    // --- Final Signal Generation ---
    std::vector<double> closes;
    for (const auto& c : candles) closes.push_back(c.close);

    bool use_volume = hasVolumeData(candles);
    double sma_short = computeSMA(closes, closes.size() - 1, optimal_params.sma_short);
    double sma_long = computeSMA(closes, closes.size() - 1, optimal_params.sma_long);
    double rsi = computeRSI(closes, closes.size() - 1, optimal_params.rsi_period);
    double atr = computeATR(candles, candles.size() - 1, optimal_params.atr_period);
    int obv_direction = 0;

    if (use_volume) {
        obv_direction = computeOBVDirection(candles, 14); // Check OBV over 14 periods
        output_stream << "Volume data detected. Activating OBV indicator." << std::endl;
    } else {
        output_stream << "No volume data detected. Using price-only indicators." << std::endl;
    }

    std::string signal = "HOLD";
    if (use_volume) {
        if (sma_short > sma_long && rsi > 50 && obv_direction == 1) signal = "BUY";
        else if (sma_short < sma_long && rsi < 50 && obv_direction == -1) signal = "SELL";
    } else {
        if (sma_short > sma_long && rsi > 50) signal = "BUY";
        else if (sma_short < sma_long && rsi < 50) signal = "SELL";
    }

    // --- Output and Logging ---
    double entry = candles.back().close;
    double sl = 0, tp = 0;

    output_stream << "FINAL SIGNAL: " << candles.back().datetime << " | " << cfg.ticker << " | " << signal;

    if (signal != "HOLD") {
        sl = (signal == "BUY") ? entry - 1.5 * atr : entry + 1.5 * atr;
        tp = (signal == "BUY") ? entry + 2.0 * atr : entry - 2.0 * atr;
        output_stream << " | Entry=" << entry << " SL=" << sl << " TP=" << tp;
        logTrade(candles.back().datetime, cfg.ticker, signal, entry, sl, tp);
    }

    if (use_volume) output_stream << " | OBV Dir=" << obv_direction;
    output_stream << std::endl;

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
    // process ticker
    for (const auto& cfg : cfgs) {
    workers.emplace_back(ProcessTicker, cfg);
    }

    std::cout << "Launched " << workers.size() << " worker threads. Waiting for completion..." << std::endl;

    // Wait for all threads to finish
    for (auto& worker : workers) {
        worker.join(); // This will pause the main function until the thread is done.
    }

    std::cout << "\n--- All tasks complete. ---" << std::endl;

    return 0;
}
