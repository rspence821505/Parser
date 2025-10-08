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

/**
 * @class CSVAnalyzer
 * @brief Main application class for parsing and analyzing financial CSV data
 *
 * This class orchestrates the entire analysis pipeline:
 * 1. Reads CSV files line by line
 * 2. Parses each line into structured data
 * 3. Maintains separate indicator series for each symbol
 * 4. Outputs results in CSV format with selected indicators
 *
 * The analyzer supports filtering by symbol and selective output of indicators
 * based on command-line configuration.
 */
class CSVAnalyzer {
private:
  // ========== Member Variables ==========

  CLIConfig
      config; ///< Command-line configuration controlling analysis behavior

  /**
   * @brief Map of symbol names to their corresponding indicator series
   *
   * Each unique symbol gets its own Series object that maintains independent
   * indicator state. This allows simultaneous analysis of multiple symbols
   * in a single pass through the data.
   */
  std::unordered_map<std::string, Series> symbol_data;

  ParseStats stats; ///< Statistics tracking parsing success/failure (currently
                    ///< unused but available for future logging)

public:
  /**
   * @brief Constructs a CSVAnalyzer with the given configuration
   * @param cli_config Configuration object containing analysis parameters and
   * output flags
   */
  CSVAnalyzer(const CLIConfig &cli_config) : config(cli_config) {}

  /**
   * @brief Splits a CSV line into four fields without string allocation
   * @param line The CSV line to split (expected format:
   * "timestamp,symbol,price,volume")
   * @return Array of 4 FieldRange objects pointing to substrings within the
   * original line
   *
   * This function uses pointer arithmetic to avoid string copying, making
   * parsing more efficient for large files. Each FieldRange contains a pointer
   * to the start of the field and its length.
   *
   * Expected CSV format: "2024-01-15 09:30:00,AAPL,150.25,1000000"
   * Fields: [0]=timestamp, [1]=symbol, [2]=price, [3]=volume
   */
  std::array<FieldRange, 4> split_csv_line(const std::string &line) {
    const char *data = line.data();
    std::array<FieldRange, 4> fields = {}; // Initialize all fields to zeros
    int field_index = 0;
    size_t start = 0;

    // Iterate through the line, splitting on commas
    for (size_t i = 0; i <= line.length() && field_index < 4; ++i) {
      // Found delimiter or end of line
      if (i == line.length() || line[i] == ',') {
        // Record the field's position and length
        fields[field_index] = {data + start, i - start};
        field_index++;
        start = i + 1; // Start of next field is after the comma
      }
    }

    return fields;
  }

  /**
   * @brief Parses a CSV line into a structured ParsedRow object
   * @param line The CSV line to parse
   * @return ParsedRow containing parsed data with is_valid flag set
   * appropriately
   *
   * This function performs the complete parsing pipeline:
   * 1. Splits the line into fields
   * 2. Validates field count
   * 3. Extracts timestamp and symbol as strings
   * 4. Parses price as a double using strtod (faster than std::stod)
   * 5. Parses volume as a long using std::from_chars
   *
   * Returns ParsedRow::invalid() if:
   * - Line doesn't have exactly 4 fields
   * - Price field cannot be parsed as a valid double
   * - Volume field cannot be parsed as a valid long integer
   *
   * Performance note: Uses strtod and from_chars to avoid string allocation
   * overhead
   */
  ParsedRow parse_line(const std::string &line) {
    auto fields = split_csv_line(line);

    // Validate we have all expected fields
    if (fields.size() != 4) {
      return ParsedRow::invalid();
    }

    // Extract timestamp and symbol (fields 0 and 1)
    std::string timestamp(fields[0].start, fields[0].length);
    std::string symbol(fields[1].start, fields[1].length);

    // Parse price (field 2) using strtod for efficiency
    // strtod is faster than std::stod as it works directly with char* without
    // string allocation
    char *end_ptr;
    double price = strtod(fields[2].start, &end_ptr);

    // Validate price parsing: ensure we consumed characters and reached the end
    // of the field
    if (end_ptr == fields[2].start || end_ptr != fields[2].end()) {
      return ParsedRow::invalid();
    }

    // Parse volume (field 3) using from_chars for integer parsing
    long volume;
    auto vol_result = std::from_chars(fields[3].start, fields[3].end(), volume);

    // Check for parsing errors (std::errc{} represents success)
    if (vol_result.ec != std::errc{}) {
      return ParsedRow::invalid();
    }

    // All fields parsed successfully
    return {timestamp, symbol, price, volume, true};
  }

  /**
   * @brief Retrieves or creates a Series object for the given symbol
   * @param symbol Stock symbol (e.g., "AAPL", "GOOGL")
   * @return Reference to the Series object for this symbol
   *
   * This function implements lazy initialization: Series objects are only
   * created when first needed for a symbol. All Series are created with the
   * same indicator parameters from the configuration.
   *
   * The EMA alpha value is calculated from the configured span using the
   * formula: alpha = 2 / (span + 1)
   */
  Series &get_or_create_series(const std::string &symbol) {
    // Check if we already have a Series for this symbol
    if (symbol_data.find(symbol) == symbol_data.end()) {
      // Create new Series with configured parameters
      double ema_alpha = span_to_alpha(config.ema_span);
      symbol_data.emplace(
          symbol, Series(config.sma_window, ema_alpha, config.vol_window));
    }
    return symbol_data.at(symbol);
  }

  /**
   * @brief Processes a CSV file and outputs results with computed indicators
   * @param filename Path to the CSV file to process
   * @return true if processing completed successfully, false if file cannot be
   * opened
   *
   * Processing pipeline:
   * 1. Opens the file for reading
   * 2. Prints CSV header with selected indicator columns
   * 3. Reads file line by line
   * 4. Parses each line (skipping invalid/empty lines)
   * 5. Applies symbol filtering if configured
   * 6. Updates indicators for the symbol
   * 7. Outputs the row with current indicator values
   *
   * The function is streaming: it processes one line at a time without loading
   * the entire file into memory, making it suitable for very large datasets.
   */
  bool process_file(const std::string &filename) {
    std::ifstream file(filename);

    // Validate file can be opened
    if (!file.is_open()) {
      std::cerr << "Error: Cannot open file '" << filename << "'\n";
      return false;
    }

    // Output CSV header with selected indicator columns
    print_csv_header();

    std::string line;
    // Process file line by line (streaming approach)
    while (std::getline(file, line)) {
      // Skip empty lines
      if (line.empty())
        continue;

      // Parse the line into structured data
      auto parsed_row = parse_line(line);
      if (!parsed_row.is_valid)
        continue; // Skip malformed lines

      // Apply symbol filtering if configured
      // If filter_symbol is set, only process matching symbols
      if (!config.filter_symbol.empty() &&
          parsed_row.symbol != config.filter_symbol) {
        continue;
      }

      // Get or create Series for this symbol and update indicators
      auto &series = get_or_create_series(parsed_row.symbol);
      series.update(parsed_row.price, parsed_row.volume, parsed_row.timestamp);

      // Output the row with current indicator values
      print_csv_row(parsed_row, series);
    }

    return true;
  }

  /**
   * @brief Prints the CSV header line with base columns and selected indicators
   *
   * Base columns (always present): timestamp, symbol, price, volume
   *
   * Additional indicator columns are appended based on configuration flags:
   * - sma: Simple Moving Average (if config.output_sma is true)
   * - ema: Exponential Moving Average (if config.output_ema is true)
   * - volatility: Historical volatility (if config.output_vol is true)
   * - vwap: Volume-Weighted Average Price (if config.output_vwap is true)
   */
  void print_csv_header() const {
    std::string header = "timestamp,symbol,price,volume";

    // Append indicator columns based on configuration
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

  /**
   * @brief Prints a CSV data row with base fields and requested indicator
   * values
   * @param row The parsed row containing base fields (timestamp, symbol, price,
   * volume)
   * @param series The Series object containing current indicator values for
   * this symbol
   *
   * Output format matches the header: base fields followed by indicator values
   * in the same order they appear in the header.
   *
   * Performance optimization: Reserves 256 bytes for the output string to
   * minimize memory reallocations during string concatenation.
   */
  void print_csv_row(const ParsedRow &row, const Series &series) const {
    std::string line;
    line.reserve(256); // Pre-allocate memory to avoid reallocations

    // Build base columns: timestamp,symbol,price,volume
    line = row.timestamp + "," + row.symbol + "," + std::to_string(row.price) +
           "," + std::to_string(row.volume);

    // Append indicator values based on configuration flags
    // Order must match the header printed by print_csv_header()
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

/**
 * @brief Main entry point for the CSV financial data analyzer
 * @param argc Argument count
 * @param argv Argument vector containing command-line parameters
 * @return 0 on success, 1 on error
 *
 * Command-line usage:
 *   analyzer [--sma=N] [--ema=N] [--vol=N] [--vwap=daily] [--symbol=SYM]
 * filename.csv
 *
 * Flags:
 *   --sma=N         Enable SMA output with window size N
 *   --ema=N         Enable EMA output with span N
 *   --vol=N         Enable volatility output with window size N
 *   --vwap=daily    Enable daily VWAP output
 *   --symbol=SYM    Filter output to only show symbol SYM
 *   filename.csv    Input CSV file (required)
 *
 * Example:
 *   ./analyzer --sma=20 --ema=50 --symbol=AAPL market_data.csv
 *
 * The program reads CSV data from stdin or a file, computes technical
 * indicators, and outputs results in CSV format to stdout. This allows easy
 * piping and redirection in Unix-style command pipelines.
 *
 * Error handling:
 * - Missing filename: Prints usage and returns 1
 * - File cannot be opened: Prints error and returns 1
 * - Invalid CLI arguments: Prints error message and returns 1
 * - Any other exceptions: Catches, prints error, and returns 1
 */
int main(int argc, char *argv[]) {
  try {
    // Parse command-line arguments into configuration object
    CLIConfig config = parse_cli_args(argc, argv);

    // Validate that input filename was provided
    if (config.input_filename.empty()) {
      std::cerr << "Usage: analyzer [--sma=N] [--ema=N] [--vol=N] "
                   "[--vwap=daily] [--symbol=SYM] filename.csv\n";
      return 1;
    }

    // Create analyzer with parsed configuration
    CSVAnalyzer analyzer(config);

    // Process the input file and exit with error code if processing fails
    if (!analyzer.process_file(config.input_filename)) {
      return 1;
    }

    return 0; // Success

  } catch (const std::exception &e) {
    // Catch any exceptions and report them to stderr
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}