import pandas as pd
import sys

def read_analysis_config(config_file):
    """Reads the analyze_conf.txt file to get spread, lot size, and point value for each ticker."""
    config = {}
    try:
        with open(config_file, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"): continue
                parts = line.split()
                if len(parts) == 4:
                    ticker, spread, lot_size, point_value = parts
                    config[ticker] = {
                        'spread': float(spread),
                        'lot_size': float(lot_size),
                        'point_value': float(point_value)
                    }
    except FileNotFoundError:
        print(f"Warning: Analysis config file '{config_file}' not found. Calculations will be incorrect.")
    return config

def analyze_trades(trade_log, analysis_config_file):
    """
    Simulates trades from a DataFrame and calculates performance.
    """
    analysis_config = read_analysis_config(analysis_config_file)

    if trade_log.empty:
        print("Trade log is empty or was filtered to be empty. No trades to analyze.")
        return

    wins, losses = 0, 0
    total_closed_pnl, unclosed_order_pnl = 0.0, 0.0

    print("--- Starting Live Test Simulation (with Full Config) ---")

    for ticker, trades in trade_log.groupby('Ticker'):
        print(f"\nAnalyzing trades for {ticker}...")

        config = analysis_config.get(ticker, {'spread': 0.0, 'lot_size': 0.0, 'point_value': 0.0})
        spread = config['spread']
        lot_size = config['lot_size']
        point_value = config['point_value']

        if lot_size == 0.0 or point_value == 0.0:
            print(f"  - Warning: Incomplete config for {ticker}. P/L will be 0.")
        else:
            print(f"  - Using Spread: {spread}, Lot Size: {lot_size}, Point Value: {point_value}")

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

            outcome, pnl_points, trade_closed = "INCOMPLETE", 0.0, False

            for i in range(start_index + 1, len(price_data)):
                candle = price_data.iloc[i]

                if trade['Signal'] == 'BUY':
                    if candle['Low'] <= trade['StopLoss']:
                        pnl_points = (trade['StopLoss'] - trade['Entry'])
                        outcome, losses, trade_closed = "LOSS", losses + 1, True; break
                    elif candle['High'] >= trade['TakeProfit']:
                        pnl_points = (trade['TakeProfit'] - trade['Entry'])
                        outcome, wins, trade_closed = "WIN", wins + 1, True; break
                elif trade['Signal'] == 'SELL':
                    if candle['High'] >= trade['StopLoss']:
                        pnl_points = (trade['Entry'] - trade['StopLoss'])
                        outcome, losses, trade_closed = "LOSS", losses + 1, True; break
                    elif candle['Low'] <= trade['TakeProfit']:
                        pnl_points = (trade['Entry'] - trade['TakeProfit'])
                        outcome, wins, trade_closed = "WIN", wins + 1, True; break

            if not trade_closed:
                if trade['Signal'] == 'BUY': pnl_points = (last_close_price - trade['Entry'])
                elif trade['Signal'] == 'SELL': pnl_points = (trade['Entry'] - last_close_price)

                pnl = (pnl_points - spread) * lot_size * point_value
                unclosed_order_pnl += pnl
                print(f"  - Trade at {trade['Datetime'].strftime('%Y-%m-%d %H:%M')}: {trade['Signal']} -> OPEN | Current PnL: ${pnl:.2f}")
            else:
                pnl = (pnl_points - spread) * lot_size * point_value
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

def print_help():
    """Prints the help message for the script."""
    print("Usage: python live_analyze_v7.py [OPTION] [TICKER]")
    print("Analyzes the trade log and calculates performance metrics.")
    print("\nOptions:")
    print("  <no arguments>      Analyze all tickers found in the trade log.")
    print("  <TICKER>            Analyze only the specified ticker (e.g., JPY=X).")
    print("  -e <TICKER>         Exclude the specified ticker from the analysis.")
    print("  -h, --help          Display this help message and exit.")

if __name__ == "__main__":
    tradelog_file = "tradelog.csv"
    analysis_config_file = "analyze_conf.txt"

    args = sys.argv[1:]

    # --- NEW: Argument Parsing Logic with Help ---
    if '-h' in args or '--help' in args:
        print_help()
        sys.exit(0) # Exit successfully after showing help

    try:
        trade_log = pd.read_csv(tradelog_file)
        trade_log['Datetime'] = pd.to_datetime(trade_log['Datetime'])
    except FileNotFoundError:
        print(f"Error: Trade log file '{tradelog_file}' not found.")
        sys.exit(1)

    if len(args) == 0:
        print("No arguments provided. Analyzing all tickers.")
        # No changes needed, use the full trade_log
        pass
    elif len(args) == 1:
        ticker_to_run = args[0]
        print(f"Filtering analysis for ticker: {ticker_to_run}")
        trade_log = trade_log[trade_log['Ticker'] == ticker_to_run]
    elif len(args) == 2 and args[0] == '-e':
        ticker_to_exclude = args[1]
        print(f"Excluding ticker from analysis: {ticker_to_exclude}")
        trade_log = trade_log[trade_log['Ticker'] != ticker_to_exclude]
    else:
        print("Error: Invalid arguments. Use -h or --help for usage information.")
        sys.exit(1)

    analyze_trades(trade_log, analysis_config_file)
