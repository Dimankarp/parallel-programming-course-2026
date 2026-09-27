#ifndef PARALLEL_GEN_H
#define PARALLEL_GEN_H

#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

namespace parallel {

class ZipfDistribution {
public:
  using result_type = uint64_t;

  ZipfDistribution(uint64_t n, double s) : n_(n), s_(s), q_(s) {
    h_integral_x1_ = h_integral(1.5) - 1.0;
    h_integral_n_ = h_integral(n_ + 0.5);
    s_val_ = 2.0 - h_integral_inv(h_integral(2.5) - h(2.0));
  }

  template <typename URNG> result_type operator()(URNG &rng) {
    while (true) {
      double u = h_integral_n_ + dist_(rng) * (h_integral_x1_ - h_integral_n_);
      double x = h_integral_inv(u);
      uint64_t k = static_cast<uint64_t>(x + 0.5);

      if (k < 1)
        k = 1;
      if (k > n_)
        k = n_;

      if (k - x <= s_val_) {
        return k;
      }

      if (u >= h_integral(k + 0.5) - h(k)) {
        return k;
      }
    }
  }

  uint64_t n() const { return n_; }
  double s() const { return s_; }

private:
  uint64_t n_;
  double s_;
  double q_;
  double h_integral_x1_;
  double h_integral_n_;
  double s_val_;
  std::uniform_real_distribution<double> dist_{0.0, 1.0};

  inline double h(double x) const { return std::pow(x, -q_); }

  inline double h_integral(double x) const {
    double log_x = std::log(x);
    return helper2((1.0 - q_) * log_x) * log_x;
  }

  inline double h_integral_inv(double x) const {
    double t = x * (1.0 - q_);
    if (t < -1.0) {
      t = -1.0;
    }
    return std::exp(helper1(t) * x);
  }

  static inline double helper1(double x) {
    if (std::abs(x) > 1e-8) {
      return std::log1p(x) / x;
    }
    return 1.0 - x * (0.5 - x / 3.0);
  }

  static inline double helper2(double x) {
    if (std::abs(x) > 1e-8) {
      return std::expm1(x) / x;
    }
    return 1.0 + x * (0.5 + x / 6.0);
  }
};

} // namespace parallel

#endif // PARALLEL_GEN_H