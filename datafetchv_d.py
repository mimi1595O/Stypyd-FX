import yfinance as yf
import pandas as pd
import os

# Define the exact header your C++ program expects, now with ATR
EXPECTED_HEADER = "Datetime,Open,High,Low,Close,Adj Close,Volume,ATR"
MAX_ROWS = 9000 # Set max rows to keep the data file lean

with open("conf.txt") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        pair = line.split()[0] # Read only the pair from conf.txt
        csv_file = f"{pair}.csv"

        print(f"Updating data for {pair} (5m)")

        # Fetch enough data to calculate ATR correctly
        new_data_df = yf.download(
            tickers=pair,
            interval="5m",
            #peroid = "30d", #UNCOMMENT WHEN GETTING NEW TICKER
            period="2d",
            progress=False,
            auto_adjust=False
        )

        if new_data_df.empty:
            print(f"No new data fetched for {pair}. Skipping.")
            continue

        new_data_df.reset_index(inplace=True)

        # --- ATR CALCULATION LOGIC ---
        high_low = new_data_df['High'] - new_data_df['Low']
        high_prev_close = abs(new_data_df['High'] - new_data_df['Close'].shift())
        low_prev_close = abs(new_data_df['Low'] - new_data_df['Close'].shift())
        tr = pd.concat([high_low, high_prev_close, low_prev_close], axis=1).max(axis=1)
        new_data_df['ATR'] = tr.rolling(window=14).mean()

        new_data_df.dropna(inplace=True)

        # --- APPENDING AND SAVING LOGIC ---
        data_map = {}
        if os.path.exists(csv_file):
            with open(csv_file, 'r') as f:
                f.readline() # Skip header
                for row_str in f:
                    row_str = row_str.strip()
                    if row_str:
                        datetime_key = row_str.split(',')[0]
                        data_map[datetime_key] = row_str

        for index, row in new_data_df.iterrows():
            datetime_key = pd.to_datetime(row.iloc[0]).strftime('%Y-%m-%d %H:%M:%S')

            # Create the CSV row string manually, using .iloc[0] for the fix
            row_str = (
                f"{datetime_key},"
                # Using .iloc[0] is the recommended, future-proof way to extract the scalar value
                f"{float(row['Open'].iloc[0]):.6f},"
                f"{float(row['High'].iloc[0]):.6f},"
                f"{float(row['Low'].iloc[0]):.6f},"
                f"{float(row['Close'].iloc[0]):.6f},"
                f"{float(row['Adj Close'].iloc[0]):.6f},"
                f"{int(row['Volume'].iloc[0])},"
                f"{float(row['ATR'].iloc[0]):.6f}"
            )
            data_map[datetime_key] = row_str

        sorted_datetimes = sorted(data_map.keys())

        if len(sorted_datetimes) > MAX_ROWS:
            sorted_datetimes = sorted_datetimes[-MAX_ROWS:]

        with open(csv_file, 'w') as f:
            f.write(EXPECTED_HEADER + '\n')
            for dt in sorted_datetimes:
                f.write(data_map[dt] + '\n')

        print(f"Successfully saved {len(sorted_datetimes)} total candles to '{csv_file}'.")

