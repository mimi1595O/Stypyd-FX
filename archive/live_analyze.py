import pandas as pd

def analyze_trades(tradelog_file):
	"""
	Simulates trades from a log file and calculates performance metrics.
	"""
	try:
		trade_log = pd.read_csv(tradelog_file)
	except FileNotFoundError:
		print(f"Error: Trade log file '{tradelog_file}' not found.")
		return

	if trade_log.empty:
		print("Trade log is empty. No trades to analyze.")
		return

	# Convert datetime strings to actual datetime objects for comparison
	trade_log['Datetime'] = pd.to_datetime(trade_log['Datetime'])

	wins = 0
	losses = 0
	total_pnl = 0.0

	print("--- Starting Live Test Simulation ---")

	# Group trades by ticker to load price data only once per ticker
	for ticker, trades in trade_log.groupby('Ticker'):
		print(f"\nAnalyzing trades for {ticker}...")
		try:
			# Load the complete price history for the ticker
			price_data = pd.read_csv(f"{ticker}.csv")
			price_data['Datetime'] = pd.to_datetime(price_data['Datetime'])
		except FileNotFoundError:
			print(f"  - Price data file '{ticker}.csv' not found. Skipping trades for this ticker.")
			continue

		for _, trade in trades.iterrows():
			# Find the exact candle when the trade was opened
			entry_index = price_data[price_data['Datetime'] == trade['Datetime']].index
			if entry_index.empty:
				print(f"  - Could not find entry candle for trade at {trade['Datetime']}. Skipping.")
				continue

			start_index = entry_index[0]

			# Simulate what happened after the trade was opened
			outcome = "INCOMPLETE"
			pnl = 0.0
			for i in range(start_index + 1, len(price_data)):
				candle = price_data.iloc[i]

				if trade['Signal'] == 'BUY':
					# Check for Stop Loss FIRST (more conservative)
					if candle['Low'] <= trade['StopLoss']:
						outcome = "LOSS"
						pnl = trade['StopLoss'] - trade['Entry']
						losses += 1
						break
						# Only if SL wasn't hit, check for Take Profit
					elif candle['High'] >= trade['TakeProfit']:
						outcome = "WIN"
						pnl = trade['TakeProfit'] - trade['Entry']
						wins += 1
						break
				elif trade['Signal'] == 'SELL':
					# Check for Stop Loss FIRST (more conservative)
					if candle['High'] >= trade['StopLoss']:
						outcome = "LOSS"
						pnl = trade['Entry'] - trade['StopLoss']
						losses += 1
						break
					# Only if SL wasn't hit, check for Take Profit
					elif candle['Low'] <= trade['TakeProfit']:
						outcome = "WIN"
						pnl = trade['Entry'] - trade['TakeProfit']
						wins += 1
						break

			total_pnl += pnl
			print(f"  - Trade at {trade['Datetime'].strftime('%Y-%m-%d %H:%M')}: {trade['Signal']} -> {outcome} | PnL: {pnl:.5f}")

	# --- Final Performance Summary ---
	total_trades = wins + losses
	win_rate = (wins / total_trades) * 100 if total_trades > 0 else 0

	# Simple ROI calculation assuming you risk 1 'unit' per trade
	# A more advanced ROI would need an initial capital and position sizing
	roi = (total_pnl / losses) * 100 if losses > 0 else float('inf') if wins > 0 else 0


	print("\n--- PERFORMANCE SUMMARY ---")
	print(f"Total Trades Analyzed: {total_trades}")
	print(f"Winning Trades:        {wins}")
	print(f"Losing Trades:         {losses}")
	print(f"Win Rate:              {win_rate:.2f}%")
	print(f"Total Net PnL (points):{total_pnl:.5f}")
	print(f"ROI (based on losses): {roi:.2f}% (PnL per point lost)")
	print("---------------------------\n")


if __name__ == "__main__":
	analyze_trades("tradelog.csv")
