#include "csv.hpp"
#include "indicators.hpp"
#include <array>
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

    // Parse price and volume
    try {
      std::string price_str(fields[2].start, fields[2].length);
      std::string volume_str(fields[3].start, fields[3].length);

      double price = std::stod(price_str);
      long volume = std::stol(volume_str);

      return {timestamp, symbol, price, volume, true};
    } catch (const std::exception &) {
      return ParsedRow::invalid();
    }
  }

  Series &get_or_create_series(const std::string &symbol) {
    if (symbol_data.find(symbol) == symbol_data.end()) {
      double ema_alpha = span_to_alpha(config.ema_span);
      symbol_data.emplace(
          symbol, Series(config.sma_window, ema_alpha, config.vol_window));
    }
    return symbol_data.at(symbol);
  }

  void process_file(const std::string &filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
      std::cerr << "Error: Cannot open file '" << filename << "'\n";
      return;
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
  }

  void print_csv_header() const {
    std::cout << "timestamp, symbol, price, volume";

    if (config.output_sma) {
      std::cout << ",sma";
    }
    if (config.output_ema) {
      std::cout << ",ema";
    }
    if (config.output_vol) {
      std::cout << ",volatility";
    }
    if (config.output_vwap) {
      std::cout << ",vwap";
    }

    std::cout << std::endl;
  }

  void print_csv_row(const ParsedRow &row, const Series &series) const {
    std::cout << row.timestamp << "," << row.symbol << "," << row.price << ","
              << row.volume;
    if (config.output_sma) {
      std::cout << "," << series.get_indicator(IndicatorType::SMA);
    }
    if (config.output_ema) {
      std::cout << "," << series.get_indicator(IndicatorType::SMA);
    }
    if (config.output_vol) {
      std::cout << "," << series.get_indicator(IndicatorType::VOLATILITY);
    }
    if (config.output_vwap) {
      std::cout << "," << series.get_indicator(IndicatorType::VWAP);
    }
    std::cout << std::endl;
  }
};

int main(int argc, char *argv[]) {

  try {
    // Parse CLI arguments
    CLIConfig config = parse_cli_args(argc, argv);

    // Validate required arguments
    if (config.input_filename.empty()) {
      std::cerr << "Usage: analyzer [--sma=N] [--ema=N] [--vol=N] "
                   "[---vwap=daily] [--symbol=SYM] filename.csv\n";
      return 1;
    }
    CSVAnalyzer analyzer(config);
    analyzer.process_file(config.input_filename);

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}