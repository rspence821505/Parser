# CSV Price Analyzer

A high-performance C++ tool for calculating technical indicators from market data streams. Designed for processing large datasets (5M+ rows) with configurable indicators and symbol filtering.

## Features

- **Technical Indicators**: SMA, EMA, Rolling Volatility, VWAP
- **High Performance**: Processes 5M rows in ~12 seconds
- **Flexible CLI**: Configure indicators and parameters via command line
- **Symbol Filtering**: Process specific symbols or all symbols
- **Daily VWAP Reset**: Automatic reset at day boundaries
- **CSV Output**: Clean CSV format with only requested indicators

## Quick Start

```bash
# Compile
g++ -std=c++20 -O3 -march=native -DNDEBUG -Iinclude -o analyzer src/analyzer.cpp

# Basic usage
./analyzer --sma=20 --ema=50 --vwap=daily data.csv > results.csv

# Filter to specific symbol
./analyzer --sma=20 --symbol=AAPL data.csv > aapl_results.csv
```

## CLI Options

| Flag           | Description                            | Example         |
| -------------- | -------------------------------------- | --------------- |
| `--sma=N`      | Simple Moving Average (N periods)      | `--sma=20`      |
| `--ema=N`      | Exponential Moving Average (N periods) | `--ema=50`      |
| `--vol=N`      | Rolling Volatility (N periods)         | `--vol=30`      |
| `--vwap=daily` | Volume Weighted Average Price          | `--vwap=daily`  |
| `--symbol=SYM` | Filter to specific symbol              | `--symbol=AAPL` |

## Input Format

CSV with columns: `timestamp,symbol,price,volume`

```csv
2023-09-15 09:30:00,AAPL,150.25,1000
2023-09-15 09:30:30,AAPL,150.30,1500
2023-09-15 09:31:00,MSFT,285.20,800
```

## Output Format

CSV with requested indicators:

```csv
timestamp,symbol,price,volume,sma,ema,vwap
2023-09-15 09:30:00,AAPL,150.25,1000,150.25,150.25,150.25
2023-09-15 09:30:30,AAPL,150.30,1500,150.275,150.27,150.278
```

## Project Structure

```
csv-analyzer/
├── include/           # Header files
│   ├── csv.hpp       # CSV parsing utilities
│   └── indicators.hpp # Technical indicator implementations
├── src/
│   └── analyzer.cpp  # Main application
├── tests/
│   └── data/         # Test datasets
├── output/           # Generated results
└── README.md
```

## Performance

- **Target**: 5-10M rows in <10 seconds
- **Achieved**: 5M rows in ~12 seconds (laptop class hardware)
- **Core processing**: 2.1 seconds (indicator calculations)
- **I/O overhead**: 9.9 seconds (CSV output)

Optimizations include:

<!-- - Zero-allocation number parsing with `std::from_chars`/`strtod` -->

- Incremental indicator calculations (O(1) per update)
- Buffered string output
- Compiler optimizations (`-O3 -march=native`)

## Technical Details

### Indicator Implementations

- **SMA**: Rolling window with `std::deque`, O(1) amortized updates
- **EMA**: Exponential smoothing, O(1) updates, no history storage
- **Volatility**: Sample standard deviation of returns over rolling window
- **VWAP**: Volume-weighted price with daily reset detection

### Architecture

- **Series Class**: Orchestrates multiple indicators per symbol
- **Independent Indicators**: Separate classes for each calculation type
- **Buffer-based Parsing**: Zero-allocation CSV field extraction
- **Configuration-driven**: Calculate all indicators, output only requested ones

## Building

### Requirements

- C++17 or later
- GCC/Clang with optimization support

### Compilation

```bash
g++ -std=c++20 -O3 -march=native -DNDEBUG -Iinclude -o analyzer src/analyzer.cpp
```

### Testing

```bash
# Generate large test dataset
python3 generate_test_data.py

# Performance test
time ./analyzer --sma=20 --ema=50 --vol=30 --vwap=daily tests/data/large_test.csv > /dev/null
```

## Examples

### All Indicators

```bash
./analyzer --sma=20 --ema=50 --vol=30 --vwap=daily data.csv > full_analysis.csv
```

### AAPL Only with SMA

```bash
./analyzer --sma=20 --symbol=AAPL data.csv > aapl_sma.csv
```

### Quick VWAP Check

```bash
./analyzer --vwap=daily data.csv > vwap_only.csv
```

<!-- ## Design Decisions

- **Real-time output**: Outputs indicators as each row is processed (vs batch at end)
- **Always calculate**: All indicators computed regardless of output flags (enables library reuse)
- **Daily VWAP**: Resets based on date portion of timestamp
- **Sample volatility**: Uses n-1 denominator for unbiased variance estimate

## Future Enhancements

- Additional indicators (RSI, Bollinger Bands, MACD)
- Multiple timeframe support
- Binary output format for performance
- Multi-threaded processing for larger datasets
- Real-time streaming mode -->

---
