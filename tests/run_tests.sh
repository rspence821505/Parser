#!/bin/bash

# Simple test runner for CSV Price Analyzer
# Usage: ./tests/run_tests.sh

# set -e  # Exit on any error

echo "=== CSV Price Analyzer Test Suite ==="
echo

PASSED=0
FAILED=0

# Function to print test results
print_result() {
    if [ $1 -eq 0 ]; then
        echo "PASS: $2"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL: $2"
        FAILED=$((FAILED + 1))
    fi
}

# Check if analyzer exists
if [ ! -f "./analyzer" ]; then
    echo "Error: analyzer executable not found. Please compile first:"
    echo "g++ -std=c++20 -O3 -march=native -DNDEBUG -Iinclude -o analyzer src/analyzer.cpp"
    exit 1
fi

# Test 1: Basic functionality with known data
echo "Test 1: Basic SMA + VWAP calculation..."
./analyzer --sma=3 --vwap=daily tests/data/small_test.csv > tests/output_test1.csv 2>/dev/null
if [ $? -eq 0 ]; then
    # Check if output has expected number of lines (header + 5 data rows)
    line_count=$(wc -l < tests/output_test1.csv)
    if [ $line_count -eq 6 ]; then
        print_result 0 "Basic functionality (correct line count)"
    else
        print_result 1 "Basic functionality (expected 6 lines, got $line_count)"
    fi
else
    print_result 1 "Basic functionality (analyzer crashed)"
fi

# Test 2: CLI argument parsing  
echo "Test 2: CLI argument validation..."
./analyzer --invalid-flag tests/data/small_test.csv > /dev/null 2>&1
if [ $? -ne 0 ]; then
    print_result 0 "CLI validation (rejects invalid flags)"
else
    print_result 1 "CLI validation (should reject invalid flags)"
fi

# Test 3: Missing filename
echo "Test 3: Missing filename handling..."
./analyzer --sma=5 > /dev/null 2>&1
if [ $? -ne 0 ]; then
    print_result 0 "Missing filename (proper error handling)"
else
    print_result 1 "Missing filename (should show error)"
fi

# Test 4: Symbol filtering
echo "Test 4: Symbol filtering..."
./analyzer --sma=3 --symbol=AAPL tests/data/small_test.csv > tests/output_test4.csv 2>/dev/null
if [ $? -eq 0 ]; then
    # Check that all data rows contain AAPL
    aapl_count=$(grep -c "AAPL" tests/output_test4.csv)
    total_data_lines=$(($(wc -l < tests/output_test4.csv) - 1))  # Subtract header
    if [ $aapl_count -eq $total_data_lines ]; then
        print_result 0 "Symbol filtering (all rows contain AAPL)"
    else
        print_result 1 "Symbol filtering (found non-AAPL rows)"
    fi
else
    print_result 1 "Symbol filtering (analyzer crashed)"
fi

# Test 5: Multiple indicators
echo "Test 5: Multiple indicators..."
./analyzer --sma=3 --ema=3 --vol=3 --vwap=daily tests/data/small_test.csv > tests/output_test5.csv 2>/dev/null
if [ $? -eq 0 ]; then
    # Check that header contains all indicators
    header=$(head -1 tests/output_test5.csv)
    if [[ $header == *"sma"* ]] && [[ $header == *"ema"* ]] && [[ $header == *"volatility"* ]] && [[ $header == *"vwap"* ]]; then
        print_result 0 "Multiple indicators (all columns present)"
    else
        print_result 1 "Multiple indicators (missing columns in header)"
    fi
else
    print_result 1 "Multiple indicators (analyzer crashed)"
fi

# Test 6: Non-existent file
echo "Test 6: Non-existent file handling..."
./analyzer --sma=5 non_existent_file.csv > /dev/null 2>&1
if [ $? -ne 0 ]; then
    print_result 0 "Non-existent file (proper error handling)"
else
    print_result 1 "Non-existent file (should show error)"
fi

# Test 7: Empty indicator flags (should still work)
echo "Test 7: No indicator flags..."
./analyzer tests/data/small_test.csv > tests/output_test7.csv 2>/dev/null
if [ $? -eq 0 ]; then
    # Should produce basic output with just timestamp,symbol,price,volume
    header=$(head -1 tests/output_test7.csv)
    if [[ $header == "timestamp,symbol,price,volume" ]]; then
        print_result 0 "No indicators (basic output only)"
    else
        print_result 1 "No indicators (unexpected header: $header)"
    fi
else
    print_result 1 "No indicators (analyzer crashed)"
fi

# Test 8: Edge case - malformed CSV line handling
echo "Test 8: Malformed data handling..."
# Create a temporary file with one good line and one bad line
echo "2023-09-15 09:30:00,AAPL,150.00,1000" > tests/temp_bad_data.csv
echo "malformed,line,without,enough,fields,extra" >> tests/temp_bad_data.csv
echo "2023-09-15 09:30:30,AAPL,151.00,1000" >> tests/temp_bad_data.csv

./analyzer --sma=2 tests/temp_bad_data.csv > tests/output_test8.csv 2>/dev/null
if [ $? -eq 0 ]; then
    # Should have header + 2 good data rows (malformed line skipped)
    line_count=$(wc -l < tests/output_test8.csv)
    if [ $line_count -eq 3 ]; then
        print_result 0 "Malformed data (bad lines skipped gracefully)"
    else
        print_result 1 "Malformed data (unexpected line count: $line_count)"
    fi
else
    print_result 1 "Malformed data (analyzer crashed)"
fi
rm -f tests/temp_bad_data.csv

# Performance test (if large file exists)
if [ -f "tests/data/large_test.csv" ]; then
    echo "Test 9: Performance test (5M rows)..."
    start_time=$(date +%s)
    ./analyzer --sma=20 --ema=50 tests/data/large_test.csv > /dev/null 2>&1
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    
    if [ $duration -le 15 ]; then
        print_result 0 "Performance test (${duration}s - within acceptable range)"
    else
        print_result 1 "Performance test (${duration}s - slower than target)"
    fi
else
    echo "Skipping performance test (large_test.csv not found)"
fi

# Clean up test output files
rm -f tests/output_test*.csv

# Summary
echo
echo "=== Test Summary ==="
echo "Tests passed: $PASSED"
echo "Tests failed: $FAILED"

if [ $FAILED -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "Some tests failed."
    exit 1
fi