#include <cmath>
#include <deque>
#include <numeric>
#include <stdexcept>
#include <string>

enum class IndicatorType { SMA, EMA, VOLATILITY, VWAP };

class SMAIndicator {
  std::deque<double> prices;
  int window_size;

public:
  SMAIndicator(int window) : window_size(window) {}
  void update(double price) {
    prices.push_back(price);

    // Remove old prices if window is full
    if (prices.size() > window_size) {
      prices.pop_front();
    }
  }
  double get_value() const {
    if (prices.empty())
      return 0.0;
    return std::accumulate(prices.begin(), prices.end(), 0.0) /
           static_cast<double>(prices.size());
  }
};

class EMAIndicator {
  double current_ema = 0.0;
  double alpha;
  bool first_price = true;

public:
  EMAIndicator(double smoothing_factor) : alpha(smoothing_factor) {}
  void update(double price) {
    if (first_price) {
      current_ema = price;
      first_price = false;
    } else {
      current_ema = alpha * price + (1 - alpha) * current_ema;
    }
  }
  double get_value() const { return current_ema; }
};

class VolatilityIndicator {
  std::deque<double> returns;
  size_t window_size;

public:
  VolatilityIndicator(size_t window) : window_size(window) {}

  void update(double return_val) {
    returns.push_back(return_val);
    if (returns.size() > window_size) {
      returns.pop_front();
    }
  }

  double get_value() const {
    if (returns.size() < 2) {
      return 0.0;
    }

    size_t n = returns.size();
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / n;
    double sum_squared_diffs = 0.0;

    for (const double &ret : returns) { // Added const& to avoid copies
      double diff = ret - mean;
      sum_squared_diffs += diff * diff;
    }

    double variance = sum_squared_diffs / (n - 1);
    return std::sqrt(variance);
  }
};

class VWAPIndicator {
  double price_volume_sum = 0.0;
  long volume_sum = 0;
  std::string current_date = ""; // Add this missing member

public:
  void update(double price, long volume, const std::string &timestamp) {
    // Extract date portion (first 10 characters)
    std::string new_date = timestamp.substr(0, 10);

    // Check if we've moved to a new day
    if (current_date != new_date) {
      // Reset VWAP for new trading day
      price_volume_sum = 0.0;
      volume_sum = 0;
      current_date = new_date;
    }

    // Add to running totals
    price_volume_sum += price * volume;
    volume_sum += volume;
  }

  double get_value() const {
    if (volume_sum == 0) {
      return 0.0;
    }
    return price_volume_sum / volume_sum;
  }
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
        last_price(0.0) {}

  void update(double price, long volume,
              const std::string &ts) { // Fix parameter
    if (last_price == 0) {
      last_price = price;
      return;
    }

    double return_value = (price / last_price) - 1.0;

    sma.update(price);
    ema.update(price);
    volatility.update(return_value);
    vwap.update(price, volume, ts);

    last_price = price;
  }

  double get_indicator(IndicatorType type) const {
    switch (type) {
    case IndicatorType::SMA:
      return sma.get_value();
    case IndicatorType::EMA:
      return ema.get_value();
    case IndicatorType::VOLATILITY:
      return volatility.get_value();
    case IndicatorType::VWAP:
      return vwap.get_value();
    default:
      throw std::invalid_argument("Unknown Indicator Type");
    }
  }
};