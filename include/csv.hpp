#ifndef CSV_HPP
#define CSV_HPP

#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string>

/**
 * @struct CLIConfig
 * @brief Configuration structure for command-line interface parameters
 *
 * This structure manages both calculation parameters and output flags for
 * financial indicators (SMA, EMA, Volume, VWAP). All indicators are always
 * calculated internally, but only output to the user when explicitly requested
 * via command-line flags.
 */
struct CLIConfig {
  // ========== Calculation Parameters ==========
  // These values are used for internal calculations regardless of output flags

  int sma_window =
      20; ///< Window size for Simple Moving Average (default: 20 periods)
  int ema_span = 50; ///< Span parameter for Exponential Moving Average
                     ///< (default: 50 periods)
  int vol_window =
      30; ///< Window size for volume calculations (default: 30 periods)

  // ========== Output Control Flags ==========
  // These flags determine which calculated values are displayed to the user

  bool output_sma = false; ///< Flag to enable SMA output (set via --sma=N)
  bool output_ema = false; ///< Flag to enable EMA output (set via --ema=N)
  bool output_vol = false; ///< Flag to enable volume output (set via --vol=N)
  bool output_vwap =
      false; ///< Flag to enable VWAP output (set via --vwap=daily)

  // ========== Filtering and Input Options ==========

  std::string filter_symbol = "";  ///< Optional symbol filter (e.g., "AAPL");
                                   ///< empty string means no filtering
  std::string input_filename = ""; ///< Path to input CSV file
  std::string vwap_reset_period =
      ""; ///< VWAP reset period (currently only "daily" is supported)
};

/**
 * @struct ParseStats
 * @brief Statistics tracker for CSV parsing operations
 *
 * Maintains counts of total lines processed, successful parses, and failures
 * to provide feedback on data quality and parsing success rate.
 */
struct ParseStats {
  size_t total_lines = 0; ///< Total number of lines encountered in the CSV file
  size_t parsed_successfully =
      0; ///< Number of lines successfully parsed into valid data
  size_t parse_failures = 0; ///< Number of lines that failed to parse correctly
};

/**
 * @struct ParsedRow
 * @brief Represents a single parsed row from the CSV input
 *
 * Contains all fields from a CSV row along with a validity flag to indicate
 * whether the row was successfully parsed. Invalid rows can be created using
 * the static invalid() factory method.
 */
struct ParsedRow {
  std::string timestamp; ///< Timestamp of the trading data point
  std::string symbol;    ///< Stock/security symbol (e.g., "AAPL", "GOOGL")
  double price;          ///< Price value for this data point
  long volume;           ///< Trading volume for this data point
  bool is_valid;         ///< Indicates whether this row was successfully parsed

  /**
   * @brief Factory method to create an invalid ParsedRow
   * @return ParsedRow with is_valid set to false and default/empty values
   *
   * Use this method when parsing fails to create a consistent invalid row
   * object rather than throwing exceptions for each parse failure.
   */
  static ParsedRow invalid() { return {"", "", 0.0, 0, false}; }
};

/**
 * @struct FieldRange
 * @brief Represents a substring view into a CSV field without copying
 *
 * This structure provides a lightweight way to reference parts of a string
 * without allocating new memory. Used for efficient CSV parsing with
 * std::from_chars.
 */
struct FieldRange {
  const char *start; ///< Pointer to the first character of the field
  size_t length;     ///< Number of characters in the field

  /**
   * @brief Convenience method to get the end pointer
   * @return Pointer to one past the last character (for use with
   * std::from_chars)
   */
  const char *end() const { return start + length; }
};

/**
 * @brief Converts EMA span parameter to smoothing factor (alpha)
 * @param span The span parameter (number of periods)
 * @return Alpha value used in EMA calculation
 *
 * Formula: alpha = 2 / (span + 1)
 *
 * The alpha value determines how much weight recent values have in the EMA.
 * A larger span results in a smaller alpha, giving more weight to historical
 * data.
 *
 * Example: span=50 → alpha=0.0392 (3.92% weight on new value, 96.08% on
 * previous EMA)
 */
double span_to_alpha(int span) { return 2.0 / (span + 1.0); }

/**
 * @brief Parses command-line arguments into a CLIConfig structure
 * @param argc Argument count from main()
 * @param argv Argument vector from main()
 * @return CLIConfig structure populated with parsed values
 * @throws std::invalid_argument if flags are malformed or contain invalid
 * values
 *
 * Accepted flag formats:
 *   --sma=N        : Set SMA window to N and enable SMA output
 *   --ema=N        : Set EMA span to N and enable EMA output
 *   --vol=N        : Set volume window to N and enable volume output
 *   --vwap=daily   : Enable VWAP calculation with daily reset (only "daily"
 * supported)
 *   --symbol=SYM   : Filter output to only show data for symbol SYM
 *   filename       : Any non-flag argument is treated as the input filename
 *
 * Example usage:
 *   ./program --sma=20 --ema=50 --symbol=AAPL data.csv
 */
CLIConfig parse_cli_args(int argc, char *argv[]) {
  CLIConfig config;

  // Iterate through all command-line arguments (skip program name at argv[0])
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    // Check if this is a flag (starts with "--")
    if (arg.length() >= 2 && arg.substr(0, 2) == "--") {
      // Find the '=' separator between key and value
      auto eq_pos = arg.find('=');

      if (eq_pos != std::string::npos) {
        // Extract key (between "--" and "=") and value (after "=")
        std::string key = arg.substr(2, eq_pos - 2);
        std::string value = arg.substr(eq_pos + 1);

        // Parse each recognized flag
        if (key == "sma") {
          config.sma_window = std::stoi(value);
          config.output_sma = true; // Enable SMA output
        } else if (key == "ema") {
          config.ema_span = std::stoi(value);
          config.output_ema = true; // Enable EMA output
        } else if (key == "vol") {
          config.vol_window = std::stoi(value);
          config.output_vol = true; // Enable volume output
        } else if (key == "symbol") {
          config.filter_symbol = value;
        } else if (key == "vwap") {
          // Only "daily" is currently supported for VWAP
          if (value == "daily") {
            config.output_vwap = true;
          } else {
            throw std::invalid_argument("VWAP only supports 'daily'");
          }
        } else {
          // Unrecognized flag key
          throw std::invalid_argument("Unknown key: " + key);
        }
      } else {
        // Flag doesn't contain '=' separator
        throw std::invalid_argument("Invalid flag format: " + arg +
                                    " (use --key=value)");
      }
    } else {
      // Not a flag, treat as input filename
      config.input_filename = arg;
    }
  }

  return config;
}

#endif