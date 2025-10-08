#ifndef INDICATORS_HPP
#define INDICATORS_HPP

#include <cmath>
#include <deque>
#include <numeric>
#include <stdexcept>
#include <string>

/**
 * @enum IndicatorType
 * @brief Enumeration of supported technical indicator types
 *
 * Used to query specific indicator values from the Series class.
 */
enum class IndicatorType {
  SMA,        ///< Simple Moving Average
  EMA,        ///< Exponential Moving Average
  VOLATILITY, ///< Historical volatility (standard deviation of returns)
  VWAP        ///< Volume-Weighted Average Price
};

/**
 * @class SMAIndicator
 * @brief Simple Moving Average calculator using a sliding window
 *
 * Maintains a rolling window of the most recent N prices and calculates
 * their arithmetic mean. Uses a deque for efficient addition/removal at both
 * ends.
 *
 * Formula: SMA = (P1 + P2 + ... + Pn) / n
 * where P1...Pn are the n most recent prices
 */
class SMAIndicator {
  std::deque<double> prices; ///< Rolling window of recent prices
  int window_size;           ///< Maximum number of prices to maintain

public:
  /**
   * @brief Constructs an SMA indicator with specified window size
   * @param window Number of periods to include in the moving average
   */
  SMAIndicator(int window) : window_size(window) {}

  /**
   * @brief Adds a new price to the indicator
   * @param price The latest price value
   *
   * If the window is full (size == window_size), the oldest price
   * is automatically removed before adding the new one.
   */
  void update(double price) {
    prices.push_back(price);

    // Remove oldest price if window is full
    if (prices.size() > window_size) {
      prices.pop_front();
    }
  }

  /**
   * @brief Calculates the current Simple Moving Average
   * @return The average of all prices in the current window, or 0.0 if empty
   *
   * During the warm-up period (before window is full), calculates the average
   * of all available prices rather than waiting for the full window.
   */
  double get_value() const {
    if (prices.empty())
      return 0.0;

    return std::accumulate(prices.begin(), prices.end(), 0.0) /
           static_cast<double>(prices.size());
  }
};

/**
 * @class EMAIndicator
 * @brief Exponential Moving Average calculator with exponential weighting
 *
 * Gives more weight to recent prices using an exponential decay function.
 * Unlike SMA, EMA doesn't require storing historical prices since it
 * incorporates past values through its recursive calculation.
 *
 * Formula: EMA(t) = α * P(t) + (1 - α) * EMA(t-1)
 * where α is the smoothing factor (alpha), P(t) is the current price,
 * and EMA(t-1) is the previous EMA value
 */
class EMAIndicator {
  double current_ema = 0.0; ///< Current EMA value (maintained across updates)
  double alpha;             ///< Smoothing factor (0 < alpha < 1)
  bool first_price = true;  ///< Flag to detect the very first price update

public:
  /**
   * @brief Constructs an EMA indicator with specified smoothing factor
   * @param smoothing_factor Alpha value controlling the weight of recent prices
   *                         Typical calculation: α = 2 / (span + 1)
   *
   * Smaller alpha values give more weight to historical data (smoother curve).
   * Larger alpha values give more weight to recent data (more responsive).
   */
  EMAIndicator(double smoothing_factor) : alpha(smoothing_factor) {}

  /**
   * @brief Updates the EMA with a new price
   * @param price The latest price value
   *
   * On the first price, initializes EMA to that price value.
   * For subsequent prices, applies the exponential smoothing formula.
   */
  void update(double price) {
    if (first_price) {
      current_ema = price;
      first_price = false;
    } else {
      current_ema = alpha * price + (1 - alpha) * current_ema;
    }
  }

  /**
   * @brief Returns the current EMA value
   * @return The most recent exponential moving average
   */
  double get_value() const { return current_ema; }
};

/**
 * @class VolatilityIndicator
 * @brief Historical volatility calculator using sample standard deviation
 *
 * Calculates the standard deviation of percentage returns over a rolling
 * window. Uses Bessel's correction (n-1 denominator) for sample variance.
 *
 * Formula: σ = sqrt( Σ(r - mean)² / (n - 1) )
 * where r represents individual returns and n is the number of returns
 */
class VolatilityIndicator {
  std::deque<double> returns; ///< Rolling window of percentage returns
  size_t window_size;         ///< Maximum number of returns to maintain

public:
  /**
   * @brief Constructs a volatility indicator with specified window size
   * @param window Number of return periods to include in calculation
   */
  VolatilityIndicator(size_t window) : window_size(window) {}

  /**
   * @brief Adds a new return value to the indicator
   * @param return_val Percentage return (e.g., 0.05 for 5% return)
   *
   * Returns should be calculated as: (current_price / previous_price) - 1.0
   * If the window is full, the oldest return is automatically removed.
   */
  void update(double return_val) {
    returns.push_back(return_val);

    if (returns.size() > window_size) {
      returns.pop_front();
    }
  }

  /**
   * @brief Calculates the current volatility (standard deviation of returns)
   * @return Standard deviation of returns in the window, or 0.0 if insufficient
   * data
   *
   * Requires at least 2 data points for meaningful calculation.
   * Uses sample standard deviation (Bessel's correction with n-1 denominator)
   * rather than population standard deviation.
   */
  double get_value() const {
    if (returns.size() < 2) {
      return 0.0;
    }

    size_t n = returns.size();

    // Calculate mean of returns
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / n;

    // Calculate sum of squared differences from mean
    double sum_squared_diffs = 0.0;
    for (const double &ret : returns) {
      double diff = ret - mean;
      sum_squared_diffs += diff * diff;
    }

    // Sample variance (using n-1 for Bessel's correction)
    double variance = sum_squared_diffs / (n - 1);

    // Return standard deviation (square root of variance)
    return std::sqrt(variance);
  }
};

/**
 * @class VWAPIndicator
 * @brief Volume-Weighted Average Price calculator with daily reset
 *
 * Calculates the average price weighted by trading volume, resetting at the
 * start of each trading day. This provides a benchmark for intraday trading
 * that accounts for the volume traded at each price level.
 *
 * Formula: VWAP = Σ(Price × Volume) / Σ(Volume)
 *
 * The calculation resets when the date portion of the timestamp changes,
 * simulating the start of a new trading session.
 */
class VWAPIndicator {
  double price_volume_sum =
      0.0;             ///< Running sum of (price * volume) for current day
  long volume_sum = 0; ///< Running sum of volume for current day
  std::string current_date =
      ""; ///< Date of current trading session (YYYY-MM-DD format)

public:
  /**
   * @brief Updates VWAP with a new trade
   * @param price Trade price
   * @param volume Trade volume
   * @param timestamp Full timestamp string (format: "YYYY-MM-DD HH:MM:SS" or
   * similar)
   *
   * Automatically detects day changes by comparing the first 10 characters
   * of the timestamp. When a new day is detected, resets all running totals.
   *
   * Example: "2024-03-15 09:30:00" → date extracted as "2024-03-15"
   */
  void update(double price, long volume, const std::string &timestamp) {
    // Extract date portion (first 10 characters: YYYY-MM-DD)
    std::string new_date = timestamp.substr(0, 10);

    // Check if we've moved to a new trading day
    if (current_date != new_date) {
      // Reset VWAP for new trading day
      price_volume_sum = 0.0;
      volume_sum = 0;
      current_date = new_date;
    }

    // Add to running totals for current day
    price_volume_sum += price * volume;
    volume_sum += volume;
  }

  /**
   * @brief Calculates the current VWAP value
   * @return Volume-weighted average price for the current day, or 0.0 if no
   * volume
   *
   * Returns 0.0 if no trades have been recorded yet for the current day
   * (prevents division by zero).
   */
  double get_value() const {
    if (volume_sum == 0) {
      return 0.0;
    }
    return price_volume_sum / volume_sum;
  }
};

/**
 * @class Series
 * @brief Aggregates multiple technical indicators for a single symbol
 *
 * This class manages all four indicator types (SMA, EMA, Volatility, VWAP)
 * simultaneously, updating them all with each new price point. It also
 * maintains state needed for return calculations (previous price).
 *
 * The Series class handles:
 * - Calculating returns from price changes
 * - Updating all indicators with each new data point
 * - Skipping the first price (no previous price to calculate return)
 * - Providing a unified interface to query any indicator
 */
class Series {
  SMAIndicator sma;               ///< Simple Moving Average indicator
  EMAIndicator ema;               ///< Exponential Moving Average indicator
  VolatilityIndicator volatility; ///< Volatility (standard deviation) indicator
  VWAPIndicator vwap;             ///< Volume-Weighted Average Price indicator
  double last_price;              ///< Previous price for return calculation

public:
  /**
   * @brief Constructs a Series with specified indicator parameters
   * @param sma_window Window size for SMA calculation
   * @param ema_alpha Smoothing factor (alpha) for EMA calculation
   * @param vol_window Window size for volatility calculation
   *
   * Initializes all four indicators and sets last_price to 0.0 to indicate
   * that no price has been processed yet.
   */
  Series(int sma_window, double ema_alpha, int vol_window)
      : sma(sma_window), ema(ema_alpha), volatility(vol_window),
        last_price(0.0) {}

  /**
   * @brief Updates all indicators with a new data point
   * @param price Current price value
   * @param volume Trading volume for this data point
   * @param ts Timestamp string (used for VWAP daily reset detection)
   *
   * Processing sequence:
   * 1. First price: Store as last_price and return (no return can be
   * calculated)
   * 2. Subsequent prices: Calculate return, update all indicators, update
   * last_price
   *
   * The return calculation is: (current_price / previous_price) - 1.0
   * Example: If price goes from 100 to 105, return = (105/100) - 1.0 = 0.05
   * (5%)
   */
  void update(double price, long volume, const std::string &ts) {
    // Handle first price: no previous price to calculate return
    if (last_price == 0) {
      last_price = price;
      return;
    }

    // Calculate percentage return from previous price
    double return_value = (price / last_price) - 1.0;

    // Update all indicators
    sma.update(price);
    ema.update(price);
    volatility.update(return_value);
    vwap.update(price, volume, ts);

    // Store current price for next return calculation
    last_price = price;
  }

  /**
   * @brief Retrieves the current value of a specific indicator
   * @param type The indicator type to query (SMA, EMA, VOLATILITY, or VWAP)
   * @return Current value of the requested indicator
   * @throws std::invalid_argument if an unknown indicator type is requested
   *
   * Example usage:
   *   double current_sma = series.get_indicator(IndicatorType::SMA);
   */
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

#endif