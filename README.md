# Stypyd-FX
stypyd-FX: A ridiculously over-engineered C++/Python trading bot. Features a multithreaded, adaptive engine that's probably smarter than me. An AI wrote the README because I was busy. Don't expect comments.


Stypyd-FX
*(Yes, it's pronounced "stupid-FX")*

Welcome. This is a thing that tries to predict financial markets. It's written in C++ for when you need to calculate a billion things in a nanosecond, and Python for when you need to politely ask the internet for data.

---

### The Official Description (An AI Wrote This Because I'm Lazy)

stypyd-FX is an algorithmic trading signal generator featuring a multithreaded, adaptive strategy engine. It optimizes Simple Moving Average (SMA) and Relative Strength Index (RSI) parameters on the fly using a random search algorithm. For instruments with volume data, it leverages On-Balance Volume (OBV) for trend confirmation and uses a pre-calculated Average True Range (ATR) as a volatility filter to avoid trading in flat markets.

Basically, it's smarter than it looks.

### How It Works (The Gist)

1.  A Python script (`datafetch...py`) goes and gets data from Yahoo Finance. It's surprisingly robust.
2.  The multithreaded C++ program (`signal...cpp`) wakes up, sees the new data, and spawns a bunch of threads like an angry octopus.
3.  Each thread analyzes a different ticker, running a bunch of simulations to figure out the "best" parameters for right now.
4.  It spits out a BUY, SELL, or HOLD signal.
5.  If it's a BUY/SELL, it gets logged to `tradelog.csv`.
6.  Profit? Maybe.

### How to Use

Seriously, if you've cloned this, you probably know what you're doing.

1.  **Compile the C++ thing:** //I COMPILED IT FOR LINUX BUT LINUX PEOPLE WOULD COMPILE IT THEMSELVES IN MOST CASES LOL -mimi//
    ```bash
    g++ -std=c++17 -pthread -o signal signalv_final.cpp
    ```
2.  **Install the Python stuff:** //BRUH WHY AI DIDN'T TELL YOU TO USE VENV -mimi//
    ```bash
    pip install pandas yfinance
    ```
3.  **Tell it what to trade:** //IF YOU ARE USING YOUR OWN API YOU PROBABLY NEED TO REWRITE PYTHON SCRIPT -mimi//
    - Edit `conf.txt`. Put tickers in there. One per line.

4.  **Run it:**
    - First, get some data: `python datafetch_final.py`
    - Then, get some signals: `./signal conf.txt`

### 🧠 A Quick Word From The Author 

//NOT MY ACTUAL WORD TS IS AI -mimi//

Look, I don't have any real financial or accounting knowledge. Most of the "smart" stuff in here came from asking an AI what to do, and I didn't bother to check it on Google. The source code is still the only real documentation.

As for the win rate or profitability, you'll have to wait for my data or test it yourself. No guarantees it won't lose all your money.

### 📜 License

This project is licensed under the GNU General Public License v3.0.

Basically, do whatever you want with the code. I don't care. Go nuts.

### Disclaimer

This is a toy project. It is not financial advice. If you hook this up to a real money account and lose your house, that's 100% on you. Don't be stypyd.
//YES DON'T BE STYPID -mimi//
