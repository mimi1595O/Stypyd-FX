import pandas as pd

def read_spread_config(config_file):
    """Reads the analyze_conf.txt file to get the spread for each ticker."""
    spreads = {}
    try:
        with open(config_file, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()
                if len(parts) == 2:
                    spreads[parts[0]] = float(parts[1])
    except FileNotFoundError:
        print(f"Warning: Spread config file '{config_file}' not found. Spreads will be assumed as 0.")
    return spreads


def analyze_trades(tradelog_file, spread_config_file):
    """
    Simulates trades from a log file and calculates performance metrics,
    including unrealized P/L for open trades and accounting for spreads.
    """
    spreads = read_spread_config(spread_config_file)

    try:
        trade_log = pd.read_csv(tradelog_file)
    except FileNotFoundError:
        print(f"Error: Trade log file '{tradelog_file}' not found.")
        return

    if trade_log.empty:
        print("Trade log is empty. No trades to analyze.")
        return

    trade_log['Datetime'] = pd.to_datetime(trade_log['Datetime'])

    wins, losses = 0, 0
    total_closed_pnl, unclosed_order_pnl = 0.0, 0.0

    print("--- Starting Live Test Simulation (with Spreads) ---")

    for ticker, trades in trade_log.groupby('Ticker'):
        print(f"\nAnalyzing trades for {ticker}...")

        # Get the spread for the current ticker, default to 0 if not found
        spread = spreads.get(ticker, 0.0)
        if spread == 0.0:
            print(f"  - Warning: No spread found for {ticker} in config. Assuming 0.")
        else:
            print(f"  - Using spread of: {spread}")

        try:
            price_data = pd.read_csv(f"{ticker}.csv")
            price_data['Datetime'] = pd.to_datetime(price_data['Datetime'])
            last_close_price = price_data.iloc[-1]['Close']
        except (FileNotFoundError, IndexError):
            print(f"  - Price data file '{ticker}.csv' not found or empty. Skipping.")
            continue

        for _, trade in trades.iterrows():
            entry_index = price_data[price_data['Datetime'] == trade['Datetime']].index
            if entry_index.empty:
                print(f"  - Could not find entry candle for trade at {trade['Datetime']}. Skipping.")
                continue

            start_index = entry_index[0]

            outcome, pnl, trade_closed = "INCOMPLETE", 0.0, False

            for i in range(start_index + 1, len(price_data)):
                candle = price_data.iloc[i]
                if trade['Signal'] == 'BUY':
                    if candle['Low'] <= trade['StopLoss']:
                        outcome, pnl, losses, trade_closed = "LOSS", (trade['StopLoss'] - trade['Entry']) - spread, losses + 1, True
                        break
                    elif candle['High'] >= trade['TakeProfit']:
                        outcome, pnl, wins, trade_closed = "WIN", (trade['TakeProfit'] - trade['Entry']) - spread, wins + 1, True
                        break
                elif trade['Signal'] == 'SELL':
                    if candle['High'] >= trade['StopLoss']:
                        outcome, pnl, losses, trade_closed = "LOSS", (trade['Entry'] - trade['StopLoss']) - spread, losses + 1, True
                        break
                    elif candle['Low'] <= trade['TakeProfit']:
                        outcome, pnl, wins, trade_closed = "WIN", (trade['Entry'] - trade['TakeProfit']) - spread, wins + 1, True
                        break

            if not trade_closed:
                if trade['Signal'] == 'BUY':
                    pnl = (last_close_price - trade['Entry']) - spread
                elif trade['Signal'] == 'SELL':
                    pnl = (trade['Entry'] - last_close_price) - spread
                unclosed_order_pnl += pnl
                print(f"  - Trade at {trade['Datetime'].strftime('%Y-%m-%d %H:%M')}: {trade['Signal']} -> OPEN | Current PnL: {pnl:.5f}")
            else:
                total_closed_pnl += pnl
                print(f"  - Trade at {trade['Datetime'].strftime('%Y-%m-%d %H:%M')}: {trade['Signal']} -> {outcome} | Final PnL: {pnl:.5f}")

    win_rate = (wins / (wins + losses)) * 100 if (wins + losses) > 0 else 0

    print("\n--- PERFORMANCE SUMMARY ---")
    print(f"Total Closed Trades:   {wins + losses}")
    print(f"Winning Trades:        {wins}")
    print(f"Losing Trades:         {losses}")
    print(f"Win Rate:              {win_rate:.2f}%")
    print(f"Closed Trades PnL:     {total_closed_pnl:.5f} (net of spread)")
    print("---------------------------")
    print(f"Unclosed Orders P/L:   {unclosed_order_pnl:.5f} (net of spread)")
    print("===========================\n")


if __name__ == "__main__":
    analyze_trades("tradelog.csv", "analyze_conf.txt")
