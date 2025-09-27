#include "../include/csv.hpp"
#include "../include/indicators.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <deque>
#include <fstream>
#include <iostream>
#include <numeric>
#include <system_error>
#include <unordered_map>

class CSVAnalyzer {
private:
  // CLI configuration
  CLIConfig config;
  std::unordered_map<std::string, Series> symbol_data;
  ParseStats stats;

public:
  CSVAnalyzer(const CLIConfig &cli_config) : config(cli_config) {}

  std::array<FieldRange, 4> split_csv_line(const std::string &line) {
    const char *data = line.data();
    std::array<FieldRange, 4> fields = {}; // Initialize to zeros
    int field_index = 0;
    size_t start = 0;

    for (size_t i = 0; i <= line.length() && field_index < 4; ++i) {
      if (i == line.length() || line[i] == ',') {
        fields[field_index] = {data + start, i - start};
        field_index++;
        start = i + 1;
      }
    }

    return fields;
  }

  ParsedRow parse_line(const std::string &line) {
    auto fields = split_csv_line(line);

    if (fields.size() != 4) {
      return ParsedRow::invalid();
    }

    // Extract timestamp and symbol
    std::string timestamp(fields[0].start, fields[0].length);
    std::string symbol(fields[1].start, fields[1].length);

    // Parse price with strtod (faster than stod, no string allocation needed)
    char *end_ptr;
    double price = strtod(fields[2].start, &end_ptr);
    if (end_ptr == fields[2].start || end_ptr != fields[2].end()) {
      return ParsedRow::invalid();
    }

    // Parse volume with from_chars (integers work on macOS)
    long volume;
    auto vol_result = std::from_chars(fields[3].start, fields[3].end(), volume);
    if (vol_result.ec != std::errc{}) {
      return ParsedRow::invalid();
    }

    return {timestamp, symbol, price, volume, true};
  }
  Series &get_or_create_series(const std::string &symbol) {
    if (symbol_data.find(symbol) == symbol_data.end()) {
      double ema_alpha = span_to_alpha(config.ema_span);
      symbol_data.emplace(
          symbol, Series(config.sma_window, ema_alpha, config.vol_window));
    }
    return symbol_data.at(symbol);
  }

  bool process_file(const std::string &filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
      std::cerr << "Error: Cannot open file '" << filename << "'\n";
      return false;
    }

    // Print CSV header
    print_csv_header();

    std::string line;
    while (std::getline(file, line)) {
      if (line.empty())
        continue;

      auto parsed_row = parse_line(line);
      if (!parsed_row.is_valid)
        continue;

      // Apply symbol filtering
      if (!config.filter_symbol.empty() &&
          parsed_row.symbol != config.filter_symbol) {
        continue;
      }

      auto &series = get_or_create_series(parsed_row.symbol);
      series.update(parsed_row.price, parsed_row.volume, parsed_row.timestamp);

      // Output the row with current indicator values
      print_csv_row(parsed_row, series);
    }
    return true;
  }

  void print_csv_header() const {
    std::string header = "timestamp,symbol,price,volume";

    if (config.output_sma) {
      header += ",sma";
    }
    if (config.output_ema) {
      header += ",ema";
    }
    if (config.output_vol) {
      header += ",volatility";
    }
    if (config.output_vwap) {
      header += ",vwap";
    }

    std::cout << header << '\n';
  }

  void print_csv_row(const ParsedRow &row, const Series &series) const {
    std::string line;
    line.reserve(256); // Reserve space to avoid reallocations

    // Build the base row data
    line = row.timestamp + "," + row.symbol + "," + std::to_string(row.price) +
           "," + std::to_string(row.volume);

    // Add indicator columns based on config
    if (config.output_sma) {
      line += "," + std::to_string(series.get_indicator(IndicatorType::SMA));
    }
    if (config.output_ema) {
      line += "," + std::to_string(series.get_indicator(IndicatorType::EMA));
    }
    if (config.output_vol) {
      line +=
          "," + std::to_string(series.get_indicator(IndicatorType::VOLATILITY));
    }
    if (config.output_vwap) {
      line += "," + std::to_string(series.get_indicator(IndicatorType::VWAP));
    }

    std::cout << line << '\n';
  }
};

int main(int argc, char *argv[]) {
  try {
    // Parse CLI arguments
    CLIConfig config = parse_cli_args(argc, argv);

    // Validate required arguments
    if (config.input_filename.empty()) {
      std::cerr << "Usage: analyzer [--sma=N] [--ema=N] [--vol=N] "
                   "[--vwap=daily] [--symbol=SYM] filename.csv\n";
      return 1;
    }

    CSVAnalyzer analyzer(config);

    // Process file and exit with error code if it fails
    if (!analyzer.process_file(config.input_filename)) {
      return 1;
    }

    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}