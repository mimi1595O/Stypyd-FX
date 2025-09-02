import yfinance as yf
import pandas as pd

with open("conf.txt") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        pair, interval, lookback = line.split()
        csv_file = f"{pair}.csv"
        print(f"Fetching {pair} {interval}")
        data = yf.download(tickers=pair, interval=interval, period="7d")
        data.to_csv(csv_file)

