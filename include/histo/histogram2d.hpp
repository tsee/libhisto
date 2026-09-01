// Owning 2D Histogram RAII handle and operations for libhisto.
// Compliant with Google C++ style guide with zero runtime and memory overhead.

#ifndef LIBHISTO_HISTOGRAM2D_HPP
#define LIBHISTO_HISTOGRAM2D_HPP

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "histo/histo2d.h"
#include "histo/histogram.hpp"
#include "histo/histogram2d_view.hpp"
#include "histo/status.hpp"
#include "histo/types.h"

namespace libhisto {

/**
 * @brief Owning RAII handle for a 2D bivariate histogram.
 */
class Histogram2D : public Histogram2DView {
 public:
  constexpr Histogram2D() noexcept : Histogram2DView(nullptr) {}
  explicit constexpr Histogram2D(histo2d_t* handle) noexcept : Histogram2DView(handle) {}

  // Direct Constructors (Uniform - Uniform)
  Histogram2D(uint32_t nx, double xmin, double xmax,
              uint32_t ny, double ymin, double ymax,
              uint32_t flags = 0) noexcept
      : Histogram2DView(histo2d_create_uniform(nx, xmin, xmax, ny, ymin, ymax, flags)) {}

  // Direct Constructors (Uniform - Variable)
  Histogram2D(uint32_t nx, double xmin, double xmax,
              Span<const double> y_edges, uint32_t flags = 0) noexcept
      : Histogram2DView(y_edges.size() > 1
                        ? histo2d_create_uniform_variable(nx, xmin, xmax,
                                                          static_cast<uint32_t>(y_edges.size() - 1),
                                                          y_edges.data(), flags)
                        : nullptr) {}

  // Direct Constructors (Variable - Uniform)
  Histogram2D(Span<const double> x_edges,
              uint32_t ny, double ymin, double ymax,
              uint32_t flags = 0) noexcept
      : Histogram2DView(x_edges.size() > 1
                        ? histo2d_create_variable_uniform(static_cast<uint32_t>(x_edges.size() - 1),
                                                          x_edges.data(), ny, ymin, ymax, flags)
                        : nullptr) {}

  // Direct Constructors (Variable - Variable)
  Histogram2D(Span<const double> x_edges, Span<const double> y_edges,
              uint32_t flags = 0) noexcept
      : Histogram2DView((x_edges.size() > 1 && y_edges.size() > 1)
                        ? histo2d_create_variable(static_cast<uint32_t>(x_edges.size() - 1),
                                                  x_edges.data(),
                                                  static_cast<uint32_t>(y_edges.size() - 1),
                                                  y_edges.data(), flags)
                        : nullptr) {}

  // Static Factory Methods
  static Result<Histogram2D> UniformUniform(uint32_t nx, double xmin, double xmax,
                                            uint32_t ny, double ymin, double ymax,
                                            uint32_t flags = 0) noexcept {
    histo2d_t* h = histo2d_create_uniform(nx, xmin, xmax, ny, ymin, ymax, flags);
    if (!h) return Status(StatusCode::kInvalidArgument);
    return Histogram2D(h);
  }

  static Result<Histogram2D> UniformVariable(uint32_t nx, double xmin, double xmax,
                                             Span<const double> y_edges,
                                             uint32_t flags = 0) noexcept {
    if (y_edges.size() < 2) return Status(StatusCode::kInvalidArgument);
    histo2d_t* h = histo2d_create_uniform_variable(nx, xmin, xmax,
                                                   static_cast<uint32_t>(y_edges.size() - 1),
                                                   y_edges.data(), flags);
    if (!h) return Status(StatusCode::kInvalidArgument);
    return Histogram2D(h);
  }

  static Result<Histogram2D> VariableUniform(Span<const double> x_edges,
                                             uint32_t ny, double ymin, double ymax,
                                             uint32_t flags = 0) noexcept {
    if (x_edges.size() < 2) return Status(StatusCode::kInvalidArgument);
    histo2d_t* h = histo2d_create_variable_uniform(static_cast<uint32_t>(x_edges.size() - 1),
                                                   x_edges.data(), ny, ymin, ymax, flags);
    if (!h) return Status(StatusCode::kInvalidArgument);
    return Histogram2D(h);
  }

  static Result<Histogram2D> VariableVariable(Span<const double> x_edges,
                                              Span<const double> y_edges,
                                              uint32_t flags = 0) noexcept {
    if (x_edges.size() < 2 || y_edges.size() < 2) return Status(StatusCode::kInvalidArgument);
    histo2d_t* h = histo2d_create_variable(static_cast<uint32_t>(x_edges.size() - 1),
                                           x_edges.data(),
                                           static_cast<uint32_t>(y_edges.size() - 1),
                                           y_edges.data(), flags);
    if (!h) return Status(StatusCode::kInvalidArgument);
    return Histogram2D(h);
  }

  // Destructor & Move Operations
  ~Histogram2D() noexcept {
    if (mut_handle()) {
      histo2d_destroy(mut_handle());
      handle_ = nullptr;
    }
  }

  Histogram2D(Histogram2D&& other) noexcept : Histogram2DView(other.handle_) {
    other.handle_ = nullptr;
  }

  Histogram2D& operator=(Histogram2D&& other) noexcept {
    if (this != &other) {
      if (mut_handle()) {
        histo2d_destroy(mut_handle());
      }
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  Histogram2D(const Histogram2D&) = delete;
  Histogram2D& operator=(const Histogram2D&) = delete;

  Histogram2D Clone(bool empty = false) const noexcept {
    if (!handle_) return Histogram2D();
    return Histogram2D(histo2d_clone(handle_, empty));
  }

  histo2d_t* release() noexcept {
    histo2d_t* h = mut_handle();
    handle_ = nullptr;
    return h;
  }

  // Ingestion
  Status Fill(double x, double y) noexcept {
    if (!handle_) return Status(StatusCode::kInvalidArgument);
    return histo2d_fill(mut_handle(), x, y);
  }

  Status Fill(double x, double y, double weight) noexcept {
    if (!handle_) return Status(StatusCode::kInvalidArgument);
    return histo2d_fill_w(mut_handle(), x, y, weight);
  }

  Status Fill(Span<const double> x, Span<const double> y) noexcept {
    if (!handle_) return Status(StatusCode::kInvalidArgument);
    if (x.size() != y.size()) return Status(StatusCode::kInvalidArgument);
    if (x.empty()) return OkStatus();
    return histo2d_fill_n(mut_handle(), x.size(), x.data(), y.data(), nullptr);
  }

  Status Fill(Span<const double> x, Span<const double> y, Span<const double> weights) noexcept {
    if (!handle_) return Status(StatusCode::kInvalidArgument);
    if (x.size() != y.size() || x.size() != weights.size()) {
      return Status(StatusCode::kInvalidArgument);
    }
    if (x.empty()) return OkStatus();
    return histo2d_fill_n(mut_handle(), x.size(), x.data(), y.data(), weights.data());
  }

  // Mutators
  Status Reset() noexcept {
    if (!handle_) return Status(StatusCode::kInvalidArgument);
    return histo2d_reset(mut_handle());
  }

  Status Scale(double factor) noexcept {
    if (!handle_) return Status(StatusCode::kInvalidArgument);
    return histo2d_scale(mut_handle(), factor);
  }

  Status Merge(const Histogram2DView& other, double scale = 1.0) noexcept {
    if (!handle_ || !other.c_handle()) return Status(StatusCode::kInvalidArgument);
    return histo2d_add(mut_handle(), other.c_handle(), scale);
  }

  Histogram2D& operator+=(const Histogram2DView& other) noexcept {
    Merge(other, 1.0);
    return *this;
  }

  Histogram2D& operator*=(double factor) noexcept {
    Scale(factor);
    return *this;
  }

  // Serialization
  Result<std::vector<uint8_t>> Serialize() const {
    if (!handle_) return Status(StatusCode::kInvalidArgument);
    size_t needed = 0;
    histo_status_t st = histo2d_serialize_binary(handle_, nullptr, 0, &needed);
    if (st != HISTO_OK && st != HISTO_ERR_SERIALIZATION && needed == 0) {
      return Status(st);
    }
    std::vector<uint8_t> buffer(needed);
    size_t written = 0;
    st = histo2d_serialize_binary(handle_, buffer.data(), buffer.size(), &written);
    if (st != HISTO_OK) return Status(st);
    buffer.resize(written);
    return buffer;
  }

  static Result<Histogram2D> Deserialize(Span<const uint8_t> data) {
    if (data.empty()) return Status(StatusCode::kInvalidArgument);
    histo2d_t* h = nullptr;
    histo_status_t st = histo2d_deserialize_binary(data.data(), data.size(), &h);
    if (st != HISTO_OK || !h) return Status(st != HISTO_OK ? st : HISTO_ERR_DESERIALIZATION);
    return Histogram2D(h);
  }

 private:
  histo2d_t* mut_handle() noexcept {
    return const_cast<histo2d_t*>(handle_);
  }
};

}  // namespace libhisto

#endif  // LIBHISTO_HISTOGRAM2D_HPP
