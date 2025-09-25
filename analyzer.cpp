#include <array>         // std::array
#include <cmath>         // std::sqrt
#include <deque>         // std::deque
#include <fstream>       // std::ifstream
#include <iostream>      // std::cout, std::cerr, etc.
#include <numeric>       // std::accumulate
#include <system_error>  // std::errc
#include <unordered_map> // std::unordered_map

#include "csv.hpp"
#include "indicators.hpp"

class CSVAnalyzer {
private:
  // CLI configuration
  int sma_window = 0;
  double ema_alpha = 0.0;
  int vol_window = 0;
  bool calculate_vwap = false;
  std::string filter_symbol = ""; // empty means all symbols
  std::unordered_map<std::string, Series> symbol_data;
  ParseStats stats;

public:
  std::array<FieldRange, 4> split_csv_line(const std::string &line) {
    const char *data = line.data();
    const char *current = data;
    std::array<FieldRange, 4> fields;
    int field_index = 0;

    const char *field_start = current;

    // Walk through the line character by character
    for (size_t i = 0; i < line.length(); ++i) {
      if (line[i] == ',') {
        // Found comma - end of current field
        fields[field_index] = {field_start, current - field_start};
        field_index++;
        field_start = current + 1; // Next field starts after comma
      }
      current++;
    }

    // Last field (no trailing comma)
    fields[field_index] = {field_start, current - field_start};

    return fields;
  };

  ParsedRow parse_line(const std::string &line) {
    auto fields = split_csv_line(line);

    // Validate we got 4 fields
    if (fields.length != 4) {
      return ParsedRow::invalid();
    };

    // Extract timestamp and symbol
    std::string timestamp(fields[0].start, fields[0].length);
    std::string symbol(fields[1].start, fields[1].length);

    // Parse each field using std::from_chars
    // Parse price
    double price;
    auto result = std::from_chars(fields[2].start, fields[2].end(), price);
    if (result.ec != std::errc{}) {
      return ParsedRow::invalid();
    };
    // Parse volume
    long volume;
    auto vol_result = std::from_chars(fields[3].start, fields[3].end(), volume);
    if (vol_result.ec != std::errc{}) {
      return ParsedRow::invalid();
    };

    // Return valid ParsedRow
    return {timestamp, symbol, price, volume, true};
  };
  Series &get_or_create_series(const std::string &symbol) {

    if (symbol_data.find(symbol) == symbol_data.end()) {
      symbol_data.emplace(symbol, Series(sma_window, ema_alpha, vol_window));
    };
    return symbol_data[symbol];
  };

  void process_file(const std::string &filename) {

    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
      // Parse line
      auto parsed_row = parse_line(line);

      // Handle parsing failures
      if (!parsed_row.is_valid())
        continue;
      // Find/create Series
      auto &series = get_or_create_series(parsed_row.symbol);
      // Update Series
      series.update(parsed_row.price, parsed_row.volume, parsed_row.timestamp);
      // Handle output?
    };
  };
};