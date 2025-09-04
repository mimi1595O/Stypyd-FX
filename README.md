

---

# Stypyd-FX

*(yes, it’s pronounced exactly how you think, and no, I don’t regret it)*

A C++/Python trading bot that tries very hard to look smart. Multithreaded, adaptive, all the buzzwords. I didn’t write comments because if you need them, you shouldn’t be here.
Also, this README was written by an AI because I couldn’t be bothered.

---

### What This Actually Is

A Frankenstein bot that guesses market moves. C++ does the heavy lifting because Python is slow, but Python fetches the data because C++ shouldn’t have to deal with the internet.

The strategy engine tunes SMA and RSI parameters using random search (because pretending it’s “AI” makes my skin crawl). If there’s volume data, it waves OBV around like it means something. ATR keeps it from embarrassing itself in flat markets.

Basically, it’s duct tape and overconfidence.

---

### How It Works (the too-honest version)

1. Python grabs market data from Yahoo.
2. C++ wakes up, spawns threads like a caffeinated squid.
3. Each thread fiddles with parameters until something “optimal” pops out.
4. Result: BUY, SELL, HOLD.
5. If it’s BUY/SELL, it dumps into a CSV log.
6. Profit? Statistically improbable, but sure.

---

### How To Use (don’t ask me later)

1. Compile the C++ mess:

   ```bash
   g++ -std=c++17 -pthread -o signal signalv_final.cpp
   ```
2. Install the Python bits:

   ```bash
   pip install pandas yfinance
   ```
3. Edit `conf.txt` with your tickers. If you want to use your own API, congrats, you get to rewrite the script.
4. Run it:

   * Fetch data: `python datafetch_final.py`
   * Generate signals: `./signal conf.txt`

---

### Important Nonsense

I’m not a financial expert, I barely checked if half the formulas are right, and most of the “smarts” were AI-suggested. The only real documentation is the code itself.

If you connect this to real money and it sets your savings on fire, that’s on you.

License: GPLv3. Do whatever you want, but don’t blame me when it blows up.

---
