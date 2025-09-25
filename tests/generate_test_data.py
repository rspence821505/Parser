#!/usr/bin/env python3
import random
import datetime


def generate_large_csv(filename, num_rows=5000000):
    symbols = [
        "AAPL",
        "MSFT",
        "GOOGL",
        "AMZN",
        "TSLA",
        "META",
        "NVDA",
        "JPM",
        "JNJ",
        "V",
    ]
    base_prices = {
        "AAPL": 150.0,
        "MSFT": 285.0,
        "GOOGL": 136.0,
        "AMZN": 95.0,
        "TSLA": 240.0,
        "META": 180.0,
        "NVDA": 210.0,
        "JPM": 140.0,
        "JNJ": 160.0,
        "V": 230.0,
    }

    start_date = datetime.datetime(2023, 9, 15, 9, 30, 0)

    with open(filename, "w") as f:
        for i in range(num_rows):
            # Pick random symbol
            symbol = random.choice(symbols)
            base_price = base_prices[symbol]

            # Generate realistic price movement (±0.5% per tick)
            price_change = random.uniform(-0.005, 0.005)
            price = round(base_price * (1 + price_change), 2)

            # Generate realistic volume (100-5000 shares)
            volume = random.randint(100, 5000)

            # Increment timestamp by 1-3 seconds
            start_date += datetime.timedelta(seconds=random.randint(1, 3))
            timestamp = start_date.strftime("%Y-%m-%d %H:%M:%S")

            # Write CSV row
            f.write(f"{timestamp},{symbol},{price},{volume}\n")

            if i % 500000 == 0:
                print(f"Generated {i} rows...")

    print(f"Generated {num_rows} rows in {filename}")


if __name__ == "__main__":
    generate_large_csv("data/large_test.csv", 5000000)
