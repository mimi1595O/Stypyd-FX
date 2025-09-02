import yfinance as yf
import schedule
import time

def fetch_data():
    ticker = 'GC=F'
    data = yf.download(ticker, period='7d', interval='5m')
    data.to_csv('xauusd_5m_data.csv')
    print("Data fetched and saved.")

# Schedule the task every 5 minutes
schedule.every(5).minutes.do(fetch_data)

while True:
    schedule.run_pending()
    time.sleep(60)  # Wait for the next minute
