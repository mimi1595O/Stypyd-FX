import pandas as pd

def analyze_trades(tradelog_file):
    """
    Simulates trades from a log file and calculates performance metrics,
    including unrealized P/L for open trades.
    """
    try:
        trade_log = pd.read_csv(tradelog_file)
    except FileNotFoundError:
        print(f"Error: Trade log file '{tradelog_file}' not found.")
        return

    if trade_log.empty:
        print("Trade log is empty. No trades to analyze.")
        return

    trade_log['Datetime'] = pd.to_datetime(trade_log['Datetime'])

    wins = 0
    losses = 0
    total_closed_pnl = 0.0
    unclosed_order_pnl = 0.0  # New variable for unrealized P/L

    print("--- Starting Live Test Simulation ---")

    for ticker, trades in trade_log.groupby('Ticker'):
        print(f"\nAnalyzing trades for {ticker}...")
        try:
            price_data = pd.read_csv(f"{ticker}.csv")
            price_data['Datetime'] = pd.to_datetime(price_data['Datetime'])
            # Get the last known closing price for calculating unrealized P/L
            last_close_price = price_data.iloc[-1]['Close']
        except (FileNotFoundError, IndexError):
            print(f"  - Price data file '{ticker}.csv' not found or empty. Skipping trades for this ticker.")
            continue

        for _, trade in trades.iterrows():
            entry_index = price_data[price_data['Datetime'] == trade['Datetime']].index
            if entry_index.empty:
                print(f"  - Could not find entry candle for trade at {trade['Datetime']}. Skipping.")
                continue

            start_index = entry_index[0]

            outcome = "INCOMPLETE"
            pnl = 0.0
            trade_closed = False

            for i in range(start_index + 1, len(price_data)):
                candle = price_data.iloc[i]

                if trade['Signal'] == 'BUY':
                    if candle['Low'] <= trade['StopLoss']:
                        outcome, pnl, losses, trade_closed = "LOSS", trade['StopLoss'] - trade['Entry'], losses + 1, True
                        break
                    elif candle['High'] >= trade['TakeProfit']:
                        outcome, pnl, wins, trade_closed = "WIN", trade['TakeProfit'] - trade['Entry'], wins + 1, True
                        break
                elif trade['Signal'] == 'SELL':
                    if candle['High'] >= trade['StopLoss']:
                        outcome, pnl, losses, trade_closed = "LOSS", trade['Entry'] - trade['StopLoss'], losses + 1, True
                        break
                    elif candle['Low'] <= trade['TakeProfit']:
                        outcome, pnl, wins, trade_closed = "WIN", trade['Entry'] - trade['TakeProfit'], wins + 1, True
                        break

            # --- THIS IS THE NEW LOGIC ---
            if not trade_closed:
                # If the loop finished and the trade is still INCOMPLETE, calculate its current P/L
                if trade['Signal'] == 'BUY':
                    pnl = last_close_price - trade['Entry']
                elif trade['Signal'] == 'SELL':
                    pnl = trade['Entry'] - last_close_price

                # Add this unrealized P/L to our new variable
                unclosed_order_pnl += pnl
                print(f"  - Trade at {trade['Datetime'].strftime('%Y-%m-%d %H:%M')}: {trade['Signal']} -> OPEN | Current PnL: {pnl:.5f}")
            else:
                # If the trade was closed, add its final P/L to the closed total
                total_closed_pnl += pnl
                print(f"  - Trade at {trade['Datetime'].strftime('%Y-%m-%d %H:%M')}: {trade['Signal']} -> {outcome} | Final PnL: {pnl:.5f}")

    # --- Updated Performance Summary ---
    total_trades = wins + losses
    win_rate = (wins / total_trades) * 100 if total_trades > 0 else 0

    print("\n--- PERFORMANCE SUMMARY ---")
    print(f"Total Closed Trades:   {total_trades}")
    print(f"Winning Trades:        {wins}")
    print(f"Losing Trades:         {losses}")
    print(f"Win Rate:              {win_rate:.2f}%")
    print(f"Closed Trades PnL:     {total_closed_pnl:.5f}")
    print("---------------------------")
    print(f"Unclosed Orders P/L:   {unclosed_order_pnl:.5f}")
    print("===========================\n")


if __name__ == "__main__":
    analyze_trades("tradelog.csv")
