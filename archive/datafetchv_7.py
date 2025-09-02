import yfinance as yf
import pandas as pd
import os

# --- CONFIGURATION ---
MAX_ROWS = 7000

with open("conf.txt") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        pair, interval, _ = line.split()
        csv_file = f"{pair}.csv"

        print(f"Updating data for {pair} ({interval})")

        new_data = yf.download(
            tickers=pair,
            interval=interval,
            period="7d",
            progress=False,
            auto_adjust=False
        )

        if new_data.empty:
            print(f"No new data fetched for {pair}. Skipping.")
            continue

        new_data.reset_index(inplace=True)

        # --- FIX: More robustly find and rename the datetime column ---
        # The first column after reset_index() is always the timestamp.
        # This avoids issues where the column isn't named 'index' or 'Date'.
        datetime_col_name = new_data.columns[0]
        new_data.rename(columns={datetime_col_name: 'Datetime'}, inplace=True)
        # --- End of Fix ---

        if pd.api.types.is_datetime64_any_dtype(new_data['Datetime']):
             new_data['Datetime'] = new_data['Datetime'].dt.tz_localize(None)

        combined_data = pd.DataFrame()

        if os.path.exists(csv_file):
            try:
                old_data = pd.read_csv(csv_file)
                if not old_data.empty:
                    # Also apply the robust renaming to old data for consistency
                    old_datetime_col = old_data.columns[0]
                    old_data.rename(columns={old_datetime_col: 'Datetime'}, inplace=True)
                    combined_data = pd.concat([old_data, new_data], ignore_index=True)
                else:
                    combined_data = new_data
            except pd.errors.EmptyDataError:
                print(f"Warning: '{csv_file}' was empty. Overwriting with new data.")
                combined_data = new_data
        else:
            combined_data = new_data

        if combined_data.empty:
            print(f"No data available for {pair} after combining. Skipping.")
            continue

        # Convert the standardized 'Datetime' column to datetime objects
        combined_data['Datetime'] = pd.to_datetime(combined_data['Datetime'])

        # De-duplicate and sort
        combined_data.drop_duplicates(subset='Datetime', keep='last', inplace=True)
        combined_data.sort_values(by='Datetime', inplace=True)

        # Trim the data if it exceeds the max row count
        if len(combined_data) > MAX_ROWS:
            print(f"'{csv_file}' has {len(combined_data)} rows. Trimming to the latest {MAX_ROWS}.")
            combined_data = combined_data.tail(MAX_ROWS)

        final_data = combined_data

        # Ensure all required columns are present
        required_cols = ["Datetime", "Open", "High", "Low", "Close", "Adj Close", "Volume"]
        final_data = final_data.reindex(columns=required_cols)

        final_data.to_csv(csv_file, index=False)
        print(f"Successfully saved {len(final_data)} total candles to '{csv_file}'.")
