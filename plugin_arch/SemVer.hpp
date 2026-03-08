// Semantic versioning and version constraints for dependency resolution.
//
// SemVer::parse("1.2.3") handles 1, 2, or 3 components.
// Dependency::parse("logger >= 2.0") parses type + version constraint.
// Plain "logger" means any version.
//
// Operators: ==, !=, <, <=, >, >=, ^ (caret/compatible), ~ (tilde/patch).

#pragma once

#include <charconv>
#include <compare>
#include <stdexcept>
#include <string>
#include <string_view>

namespace plugin_arch {

struct SemVer {
  int major = 0;
  int minor = 0;
  int patch = 0;

  constexpr auto operator<=>(const SemVer&) const = default;

  static SemVer parse(std::string_view s) {
    SemVer v;

    auto parse_int = [](std::string_view part) -> int {
      int val = 0;
      auto [ptr, ec] =
          std::from_chars(part.data(), part.data() + part.size(), val);
      if (ec != std::errc{} || ptr != part.data() + part.size()) {
        throw std::runtime_error("Invalid version component: '" +
                                 std::string(part) + "'");
      }
      if (val < 0) {
        throw std::runtime_error("Negative version component: '" +
                                 std::string(part) + "'");
      }
      return val;
    };

    auto dot1 = s.find('.');
    if (dot1 == std::string_view::npos) {
      v.major = parse_int(s);
      return v;
    }
    v.major = parse_int(s.substr(0, dot1));

    auto rest = s.substr(dot1 + 1);
    auto dot2 = rest.find('.');
    if (dot2 == std::string_view::npos) {
      v.minor = parse_int(rest);
      return v;
    }
    v.minor = parse_int(rest.substr(0, dot2));
    v.patch = parse_int(rest.substr(dot2 + 1));
    return v;
  }

  [[nodiscard]] std::string to_string() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." +
           std::to_string(patch);
  }
};

// A dependency on a service type with an optional version constraint.
// Parsed from strings like "logger", "logger >= 2.0", "logger ^1.2.3".
struct Dependency {
  std::string type;

  enum class Op { any, eq, ne, lt, le, gt, ge, caret, tilde };
  Op op = Op::any;
  SemVer version;

  [[nodiscard]] bool satisfied_by(const SemVer& v) const {
    switch (op) {
      case Op::any: return true;
      case Op::eq:  return v == version;
      case Op::ne:  return v != version;
      case Op::lt:  return v < version;
      case Op::le:  return v <= version;
      case Op::gt:  return v > version;
      case Op::ge:  return v >= version;
      case Op::caret: return caret_match(v);
      case Op::tilde: return tilde_match(v);
    }
    return false;
  }

  static Dependency parse(std::string_view s) {
    Dependency dep;

    // Trim
    while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    while (!s.empty() && s.back() == ' ') s.remove_suffix(1);

    if (s.empty()) {
      throw std::runtime_error("Empty dependency string");
    }

    // Split on first space
    auto space = s.find(' ');
    if (space == std::string_view::npos) {
      dep.type = std::string(s);
      dep.op = Op::any;
      return dep;
    }

    dep.type = std::string(s.substr(0, space));
    auto rest = s.substr(space);

    while (!rest.empty() && rest.front() == ' ') rest.remove_prefix(1);
    if (rest.empty()) {
      dep.op = Op::any;
      return dep;
    }

    // Parse operator
    if (rest.starts_with(">=")) {
      dep.op = Op::ge;
      rest.remove_prefix(2);
    } else if (rest.starts_with("<=")) {
      dep.op = Op::le;
      rest.remove_prefix(2);
    } else if (rest.starts_with("!=")) {
      dep.op = Op::ne;
      rest.remove_prefix(2);
    } else if (rest.starts_with("==")) {
      dep.op = Op::eq;
      rest.remove_prefix(2);
    } else if (rest.starts_with(">")) {
      dep.op = Op::gt;
      rest.remove_prefix(1);
    } else if (rest.starts_with("<")) {
      dep.op = Op::lt;
      rest.remove_prefix(1);
    } else if (rest.starts_with("^")) {
      dep.op = Op::caret;
      rest.remove_prefix(1);
    } else if (rest.starts_with("~")) {
      dep.op = Op::tilde;
      rest.remove_prefix(1);
    } else {
      throw std::runtime_error("Unknown version operator in dependency: '" +
                               std::string(s) + "'");
    }

    while (!rest.empty() && rest.front() == ' ') rest.remove_prefix(1);
    if (rest.empty()) {
      throw std::runtime_error("Missing version after operator in: '" +
                               std::string(s) + "'");
    }

    dep.version = SemVer::parse(rest);
    return dep;
  }

 private:
  // ^1.2.3 matches >=1.2.3 and <2.0.0
  // ^0.2.3 matches >=0.2.3 and <0.3.0
  // ^0.0.3 matches >=0.0.3 and <0.0.4
  [[nodiscard]] bool caret_match(const SemVer& v) const {
    if (v < version) return false;
    if (version.major != 0) return v.major == version.major;
    if (version.minor != 0) return v.major == 0 && v.minor == version.minor;
    return v.major == 0 && v.minor == 0 && v.patch == version.patch;
  }

  // ~1.2.3 matches >=1.2.3 and <1.3.0
  [[nodiscard]] bool tilde_match(const SemVer& v) const {
    return v >= version && v.major == version.major &&
           v.minor == version.minor;
  }
};

}  // namespace plugin_arch
