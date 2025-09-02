import yfinance as yf
import pandas as pd
import os

# Define the exact header your C++ program expects
EXPECTED_HEADER = "Datetime,Open,High,Low,Close,Adj Close,Volume"

with open("conf.txt") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        pair, interval, _ = line.split()
        csv_file = f"{pair}.csv"

        print(f"Updating data for {pair} ({interval})")

        # 1. Fetch new data
        new_data_df = yf.download(
            tickers=pair,
            interval=interval,
            period="7d",
            progress=False,
            auto_adjust=False
        )

        if new_data_df.empty:
            print(f"No new data fetched for {pair}. Skipping.")
            continue

        new_data_df.reset_index(inplace=True)
        # Force the column names to be exactly what we expect
        new_data_df.columns = EXPECTED_HEADER.split(',')

        # 2. Use a dictionary for de-duplication
        data_map = {}

        # 3. Read old data into the map (if it exists)
        if os.path.exists(csv_file):
            with open(csv_file, 'r') as f:
                header = f.readline() # Read and discard header
                for row_str in f:
                    row_str = row_str.strip()
                    if row_str:
                        # The datetime is always the first part of the row
                        datetime_key = row_str.split(',')[0]
                        data_map[datetime_key] = row_str

        # 4. Manually format and add new data to the map
        for index, row in new_data_df.iterrows():
            # Format the datetime consistently to avoid timezone issues and duplicates
            datetime_key = pd.to_datetime(row['Datetime']).strftime('%Y-%m-%d %H:%M:%S')

            # Create the CSV row string manually
            row_str = (
                f"{datetime_key},"
                f"{row['Open']:.6f},"
                f"{row['High']:.6f},"
                f"{row['Low']:.6f},"
                f"{row['Close']:.6f},"
                f"{row['Adj Close']:.6f},"
                f"{int(row['Volume'])}"
            )
            # Add to map, overwriting duplicates
            data_map[datetime_key] = row_str

        # 5. Sort the data by datetime and write it back to the file
        sorted_datetimes = sorted(data_map.keys())

        with open(csv_file, 'w') as f:
            f.write(EXPECTED_HEADER + '\n')
            for dt in sorted_datetimes:
                f.write(data_map[dt] + '\n')

        print(f"Successfully saved {len(sorted_datetimes)} total candles to '{csv_file}'.")
