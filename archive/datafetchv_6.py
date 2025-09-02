import yfinance as yf
import pandas as pd
import os

# --- CONFIGURATION ---
# Set the maximum number of historical candles to keep in the CSV file.
# 7000 lines of M5 data is roughly 24 days.
MAX_ROWS = 7000

with open("conf.txt") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        pair, interval, _ = line.split() # The 'lookback' value is no longer needed here
        csv_file = f"{pair}.csv"

        print(f"Updating data for {pair} ({interval})")

        # Fetch the most recent data available (yfinance is limited for intraday)
        new_data = yf.download(tickers=pair, interval=interval, period="7d", progress=False)
        if new_data.empty:
            print(f"No new data fetched for {pair}. Skipping.")
            continue

        new_data.reset_index(inplace=True)

        # Standardize the datetime column name
        rename_map = {'index': 'Datetime', 'Date': 'Datetime'}
        new_data.rename(columns=rename_map, inplace=True)

        # Remove timezone for clean CSV export
        if pd.api.types.is_datetime64_any_dtype(new_data['Datetime']):
             new_data['Datetime'] = new_data['Datetime'].dt.tz_localize(None)

        # If a previous data file exists, append the new data
        if os.path.exists(csv_file):
            old_data = pd.read_csv(csv_file)
            old_data['Datetime'] = pd.to_datetime(old_data['Datetime'])
            combined_data = pd.concat([old_data, new_data], ignore_index=True)
        else:
            combined_data = new_data

        # De-duplicate and sort
        combined_data.drop_duplicates(subset='Datetime', keep='last', inplace=True)
        combined_data.sort_values(by='Datetime', inplace=True)

        # --- THIS IS THE NEW TRIMMING LOGIC ---
        if len(combined_data) > MAX_ROWS:
            print(f"'{csv_file}' has {len(combined_data)} rows. Trimming to the latest {MAX_ROWS}.")
            # Keep only the last MAX_ROWS rows
            combined_data = combined_data.tail(MAX_ROWS)

        final_data = combined_data

        # Ensure columns are in the correct order
        final_data = final_data[["Datetime", "Open", "High", "Low", "Close", "Adj Close", "Volume"]]

        # Save the updated (and possibly trimmed) data
        final_data.to_csv(csv_file, index=False)
        print(f"Successfully saved {len(final_data)} total candles to '{csv_file}'.")
