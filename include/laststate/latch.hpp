#ifndef LASTSTATE_LATCH_HPP
#define LASTSTATE_LATCH_HPP

extern "C" {
#include "latch.h"
}
namespace laststate {
class Breadcrumb {
  public:
    explicit Breadcrumb(const char *message) noexcept {
        ls_breadcrumb(message);
    }
};
class Span {
  public:
    explicit Span(uint16_t id) noexcept : id_(id), active_(ls_span_begin(id) == LS_OK) {
    }
    ~Span() noexcept {
        if (active_)
            (void)ls_span_end(id_);
    }
    Span(const Span &) = delete;
    Span &operator=(const Span &) = delete;

  private:
    uint16_t id_;
    bool active_;
};
inline void metric(const char *name, int32_t value) noexcept {
    ls_metric_i32(name, value);
}
inline void capture(const char *message, ls_severity_t severity = LS_SEVERITY_ERROR) noexcept {
    ls_capture_message(message, severity);
}
} // namespace laststate
#endif
