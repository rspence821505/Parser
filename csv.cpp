#include <array>         // std::array
#include <charconv>      // std::from_chars
#include <deque>         // std::deque
#include <fstream>       // std::ifstream
#include <iostream>      // std::cout, std::cerr, etc.
#include <stdexcept>     // std::invalid_argument
#include <string>        // std::string
#include <system_error>  // std::errc
#include <unordered_map> // std::unordered_map

enum class IndicatorType { SMA, EMA, VOLATILITY, VWAP };

struct ParseStats {
  size_t total_lines = 0;
  size_t parsed_successfully = 0;
  size_t parse_failures = 0;
};

struct ParsedRow {
  std::string timestamp;
  std::string symbol;
  double price;
  long volume;
  bool is_valid;

  // Constructor for invalid rows
  static ParsedRow invalid() { return {"", "", 0.0, 0, false}; }
};

struct FieldRange {
  const char *start;
  size_t length;

  // Helper for std::from_chars
  const char *end() const { return start + length; }
};

class SMAIndicator {
  std::deque<double> prices;
  int window_size;

public:
  SMAIndicator(int window) : window_size(window) {}
  void update(double price);
  double get_value() const;
};

class EMAIndicator {
  double current_ema = 0.0;
  double alpha;
  bool first_price = true;

public:
  EMAIndicator(double smoothing_factor) : alpha(smoothing_factor) {}
  void update(double price);
  double get_value() const;
};

class VolatilityIndicator {
  std::deque<double> returns;
  int window_size;

public:
  VolatilityIndicator(int window) : window_size(window) {};
  void update(double return_val);
  double get_value() const;
};

class VWAPIndicator {
  double price_volume_sum = 0.0;
  long volume_sum = 0;

public:
  void update(double price, long volume);
  double get_value() const;
};
class Series {
  SMAIndicator sma;
  EMAIndicator ema;
  VolatilityIndicator volatility;
  VWAPIndicator vwap;

  double last_price; // Shared state for return calculations

public:
  Series(int sma_window, double ema_alpha, int vol_window)
      : sma(sma_window), ema(ema_alpha), volatility(vol_window),
        last_price(0.0) {};

  void update(double price, long volume, std::string &ts) {

    // Handle first-time initialization
    if (last_price == 0) {
      last_price = price;
      return;
    };

    // Compute derived values (like price returns)
    double return_value = (price / last_price) - 1.0;

    // Update each indicator with appropriate data
    sma.update(price);
    ema.update(price);
    volatility.update(return_value);
    vwap.update(price, volume);

    // Update last price
    last_price = price;
  };
  double get_indicator(IndicatorType type) const {
    switch (type) {
    case ::IndicatorType::SMA:
      return sma.get_value();
    case ::IndicatorType::EMA:
      return ema.get_value();
    case ::IndicatorType::VOLATILITY:
      return volatility.get_value();
    case ::IndicatorType::VWAP:
      return vwap.get_value();
    default:
      throw std::invalid_argument("Unknown Indicator Type");
    };
  };
};

class CSVAnalyzer {
private:
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