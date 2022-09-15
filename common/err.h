#pragma once

#include <functional>
#include <variant>
#include <optional>
#include <string_view>
#include <string>
#include <fmt/core.h>

struct ErrorString {
  ErrorString() {}

  template <typename O>
  ErrorString(O&& o) requires(std::is_same_v<std::remove_reference<O>, ErrorString>)
    : extra(o.extra), view(o.view) {}

  template <typename S>
  ErrorString(S&& s): view(s) {}

  static ErrorString copy(const auto&& s) {
    ErrorString ret;
    ret.extra = s;
    return ret;
  }

  operator std::string_view () const {
    return extra.empty() ? view : extra;
  }

protected:
  std::string extra;
  std::string_view view;
};

struct Error {
  int code;
  ErrorString msg;

  Error(const ErrorString& msg): code(0), msg(msg) {}
  explicit Error(int code = 0, const ErrorString& msg = {}): code(code), msg(msg) {}

  template <typename... Args>
  static Error format(int code, fmt::format_string<Args...> format_str, Args&&... args) {
    return Error(code, ErrorString::copy(fmt::format(format_str, std::forward<Args>(args)...)));
  }

  template <typename... Args>
  static Error format(fmt::format_string<Args...> format_str, Args&&... args) {
    return Error(ErrorString::copy(fmt::format(format_str, std::forward<Args>(args)...)));
  }

  auto what() const {
    if(!code) return fmt::format("Error: {}", msg);
    return fmt::format("Error {}: {}", code, msg);
  }
};

// roughly based on SerenityOS ErrorOr

template <typename T, typename ErrorT = Error>
struct [[nodiscard]] ErrorOr: std::variant<T, ErrorT> {
  using std::variant<T, ErrorT>::variant;

  template <typename U>
  ErrorOr(U&& u) requires(std::is_same_v<std::remove_reference<U>, T>)
    : std::variant<T, ErrorT>(std::forward<U>(u)) {}

  bool is_error() { return std::holds_alternative<ErrorT>(*this); }
  ErrorT error() { return std::get<Error>(*this); }
  T&& release_value() { return std::move(std::get<T>(*this)); }
};

template <typename ErrorT>
struct [[nodiscard]] ErrorOr<void, ErrorT>: std::optional<ErrorT> {
  using std::optional<ErrorT>::optional;

  bool is_error() { return this->has_value(); }
  ErrorT error() { return this->value(); }
  void release_value() {}
};

template <typename T, typename ErrorT>
struct [[nodiscard]] ErrorOr<T&, ErrorT>: ErrorOr<std::reference_wrapper<T>, ErrorT> {
  using ErrorOr<std::reference_wrapper<T>, ErrorT>::ErrorOr;
};

#define TRY(x) ({ auto ____ret = (x); if(____ret.is_error()) return ____ret.error(); ____ret.release_value(); })
#define IGNORE(x) (void) x
