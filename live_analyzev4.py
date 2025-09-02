import pandas as pd

def read_analysis_config(config_file):
    """Reads the analyze_conf.txt file to get spread and lot size for each ticker."""
    config = {}
    try:
        with open(config_file, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()
                if len(parts) == 3:
                    ticker, spread, lot_size = parts
                    config[ticker] = {'spread': float(spread), 'lot_size': float(lot_size)}
    except FileNotFoundError:
        print(f"Warning: Analysis config file '{config_file}' not found. Spreads and lot sizes will be 0.")
    return config

def get_point_value(ticker):
    """
    Determines the currency value of a single point move for a standard lot.
    This is a simplification; real-world pip values can fluctuate.
    """
    # For JPY pairs, a "pip" is at the 2nd decimal place. A standard lot is 100,000.
    # The value of a pip is (0.01 / JPY rate) * 100,000. We'll approximate this.
    if "JPY" in ticker.upper():
        return 1000.0
    # For most other pairs, a "pip" is at the 4th decimal place.
    # The value of a pip is 0.0001 * 100,000 = $10. We'll use the point value directly.
    else:
        return 100000.0

def analyze_trades(tradelog_file, analysis_config_file):
    """
    Simulates trades and calculates performance, including spreads and position sizing.
    """
    analysis_config = read_analysis_config(analysis_config_file)

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

    print("--- Starting Live Test Simulation (with Spreads & Position Sizing) ---")

    for ticker, trades in trade_log.groupby('Ticker'):
        print(f"\nAnalyzing trades for {ticker}...")

        config = analysis_config.get(ticker, {'spread': 0.0, 'lot_size': 0.0})
        spread = config['spread']
        lot_size = config['lot_size']
        point_value = get_point_value(ticker)

        if lot_size == 0.0:
            print(f"  - Warning: No config found for {ticker}. P/L will be 0.")
        else:
            print(f"  - Using Spread: {spread}, Lot Size: {lot_size}")

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
                pnl_points = 0.0

                if trade['Signal'] == 'BUY':
                    if candle['Low'] <= trade['StopLoss']:
                        pnl_points = (trade['StopLoss'] - trade['Entry']) - spread
                        outcome, losses, trade_closed = "LOSS", losses + 1, True
                        break
                    elif candle['High'] >= trade['TakeProfit']:
                        pnl_points = (trade['TakeProfit'] - trade['Entry']) - spread
                        outcome, wins, trade_closed = "WIN", wins + 1, True
                        break
                elif trade['Signal'] == 'SELL':
                    if candle['High'] >= trade['StopLoss']:
                        pnl_points = (trade['Entry'] - trade['StopLoss']) - spread
                        outcome, losses, trade_closed = "LOSS", losses + 1, True
                        break
                    elif candle['Low'] <= trade['TakeProfit']:
                        pnl_points = (trade['Entry'] - trade['TakeProfit']) - spread
                        outcome, wins, trade_closed = "WIN", wins + 1, True
                        break

            # Calculate final P/L in currency
            pnl = pnl_points * lot_size * point_value

            if not trade_closed:
                pnl_points_open = 0.0
                if trade['Signal'] == 'BUY':
                    pnl_points_open = (last_close_price - trade['Entry']) - spread
                elif trade['Signal'] == 'SELL':
                    pnl_points_open = (trade['Entry'] - last_close_price) - spread

                pnl = pnl_points_open * lot_size * point_value
                unclosed_order_pnl += pnl
                print(f"  - Trade at {trade['Datetime'].strftime('%Y-%m-%d %H:%M')}: {trade['Signal']} -> OPEN | Current PnL: ${pnl:.2f}")
            else:
                total_closed_pnl += pnl
                print(f"  - Trade at {trade['Datetime'].strftime('%Y-%m-%d %H:%M')}: {trade['Signal']} -> {outcome} | Final PnL: ${pnl:.2f}")

    win_rate = (wins / (wins + losses)) * 100 if (wins + losses) > 0 else 0

    print("\n--- PERFORMANCE SUMMARY ---")
    print(f"Total Closed Trades:   {wins + losses}")
    print(f"Winning Trades:        {wins}")
    print(f"Losing Trades:         {losses}")
    print(f"Win Rate:              {win_rate:.2f}%")
    print(f"Closed Trades PnL:     ${total_closed_pnl:.2f}")
    print("---------------------------")
    print(f"Unclosed Orders P/L:   ${unclosed_order_pnl:.2f}")
    print("===========================\n")


if __name__ == "__main__":
    analyze_trades("tradelog.csv", "analyze_conf.txt")
