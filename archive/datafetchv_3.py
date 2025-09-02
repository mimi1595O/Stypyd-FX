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

        # Fetch last 3 days
        data = yf.download(tickers=pair, interval=interval, period="3d")

        # Reset index to get datetime as a column
        data.reset_index(inplace=True)

        # Make sure columns are clean and ordered
        data = data[["Datetime", "Open", "High", "Low", "Close", "Adj Close", "Volume"]]

        # Save in the format your C++ parser expects
        data.to_csv(csv_file, index=False)
