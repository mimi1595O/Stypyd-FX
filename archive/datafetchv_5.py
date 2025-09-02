import yfinance as yf
import pandas as pd
import os

with open("conf.txt") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        pair, interval, lookback = line.split()
        csv_file = f"{pair}.csv"

        print(f"Updating data for {pair} ({interval})")

        # Fetch the most recent data available
        new_data = yf.download(tickers=pair, interval=interval, period="7d", progress=False)

        if new_data.empty:
            print(f"No new data fetched for {pair}. Skipping.")
            continue

        # Reset index to get datetime as a column, which is what the C++ parser expects
        new_data.reset_index(inplace=True)

        combined_data = pd.DataFrame()

        # If a previous data file exists, load it and append the new data
        if os.path.exists(csv_file):
            try:
                old_data = pd.read_csv(csv_file)
                # Ensure 'Datetime' column is in the correct format for merging
                old_data['Datetime'] = pd.to_datetime(old_data['Datetime'])
                new_data['Datetime'] = pd.to_datetime(new_data['Datetime'])

                combined_data = pd.concat([old_data, new_data], ignore_index=True)
            except (pd.errors.EmptyDataError, KeyError):
                 # If old file is empty or malformed, start fresh
                print(f"Warning: '{csv_file}' was empty or malformed. Overwriting.")
                combined_data = new_data
        else:
            # If no file exists, the new data is our starting point
            combined_data = new_data

        # Remove any duplicate rows (based on Datetime) and sort chronologically
        combined_data.drop_duplicates(subset='Datetime', keep='last', inplace=True)
        combined_data.sort_values(by='Datetime', inplace=True)

        # Make sure final columns are clean and in the correct order for the C++ script
        final_data = combined_data[["Datetime", "Open", "High", "Low", "Close", "Adj Close", "Volume"]]

        # Save the updated data
        final_data.to_csv(csv_file, index=False)
        print(f"Successfully saved {len(final_data)} total candles to '{csv_file}'.")
