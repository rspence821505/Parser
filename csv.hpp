
#include <charconv>  // std::from_chars
#include <stdexcept> // std::invalid_argument
#include <string>    // std::string, std::stoi

struct CLIConfig {

  // Default values - always calculate with these
  int sma_window = 20;
  int ema_span = 50;
  int vol_window = 30;

  // Output flags - only output if CLI flag specified
  bool output_sma = false;
  bool output_ema = false;
  bool output_vol = false;
  bool output_vwap = false;

  std::string filter_symbol = "";
  std::string input_filename = "";
  std::string vwap_reset_period = "";
};

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

double span_to_alpha(int span) { return 2.0 / (span + 1.0); }

CLIConfig parse_cli_args(int argc, char *argv[]) {
  CLIConfig config;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg.starts_with("--")) {
      auto eq_pos = arg.find('=');
      if (eq_pos != std::string::npos) {
        std::string key = arg.substr(2, eq_pos - 2);
        std::string value = arg.substr(eq_pos + 1);

        if (key == "sma") {
          config.sma_window = std::stoi(value);
          config.output_sma = true;
        } else if (key == "ema") {
          config.ema_span = std::stoi(value);
          config.output_ema = true;
        } else if (key == "vol") {
          config.vol_window = std::stoi(value);
          config.output_vol = true;
        } else if (key == "symbol") {
          config.filter_symbol = value;
        } else if (key == "vwap") {
          if (value == "daily") {
            config.output_vwap = true;
          } else {
            throw std::invalid_argument("VWAP only supports 'daily'");
          }
        } else {
          throw std::invalid_argument("Unknown key: " + key);
        }
      }
    } else {
      config.input_filename = arg;
    }
  }

  return config;
}
