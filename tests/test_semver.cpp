#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "SemVer.hpp"

using namespace plugin_arch;

// --- SemVer parsing ---

TEST_CASE("SemVer parse: three components", "[semver]") {
  auto v = SemVer::parse("1.2.3");
  CHECK(v.major == 1);
  CHECK(v.minor == 2);
  CHECK(v.patch == 3);
}

TEST_CASE("SemVer parse: two components", "[semver]") {
  auto v = SemVer::parse("2.5");
  CHECK(v.major == 2);
  CHECK(v.minor == 5);
  CHECK(v.patch == 0);
}

TEST_CASE("SemVer parse: one component", "[semver]") {
  auto v = SemVer::parse("7");
  CHECK(v.major == 7);
  CHECK(v.minor == 0);
  CHECK(v.patch == 0);
}

TEST_CASE("SemVer parse: zeros", "[semver]") {
  auto v = SemVer::parse("0.0.0");
  CHECK(v.major == 0);
  CHECK(v.minor == 0);
  CHECK(v.patch == 0);
}

TEST_CASE("SemVer parse: invalid throws", "[semver]") {
  CHECK_THROWS_AS(SemVer::parse("abc"), std::runtime_error);
  CHECK_THROWS_AS(SemVer::parse("1.abc"), std::runtime_error);
  CHECK_THROWS_AS(SemVer::parse(""), std::runtime_error);
}

TEST_CASE("SemVer parse: negative component throws", "[semver]") {
  CHECK_THROWS_AS(SemVer::parse("-1.0.0"), std::runtime_error);
  CHECK_THROWS_AS(SemVer::parse("1.-2.0"), std::runtime_error);
  CHECK_THROWS_AS(SemVer::parse("1.0.-3"), std::runtime_error);
}

TEST_CASE("Dependency parse: empty string throws", "[semver]") {
  CHECK_THROWS_AS(Dependency::parse(""), std::runtime_error);
  CHECK_THROWS_AS(Dependency::parse("   "), std::runtime_error);
}

// --- SemVer comparison ---

TEST_CASE("SemVer comparison: ordering", "[semver]") {
  CHECK(SemVer{1, 0, 0} < SemVer{2, 0, 0});
  CHECK(SemVer{1, 0, 0} < SemVer{1, 1, 0});
  CHECK(SemVer{1, 0, 0} < SemVer{1, 0, 1});
  CHECK(SemVer{1, 2, 3} == SemVer{1, 2, 3});
  CHECK(SemVer{2, 0, 0} > SemVer{1, 9, 9});
}

TEST_CASE("SemVer to_string", "[semver]") {
  CHECK(SemVer{1, 2, 3}.to_string() == "1.2.3");
  CHECK(SemVer{0, 0, 0}.to_string() == "0.0.0");
}

// --- Dependency parsing ---

TEST_CASE("Dependency parse: type only (any version)", "[semver]") {
  auto dep = Dependency::parse("logger");
  CHECK(dep.type == "logger");
  CHECK(dep.op == Dependency::Op::any);
}

TEST_CASE("Dependency parse: greater-equal", "[semver]") {
  auto dep = Dependency::parse("logger >= 2.0");
  CHECK(dep.type == "logger");
  CHECK(dep.op == Dependency::Op::ge);
  CHECK(dep.version == SemVer{2, 0, 0});
}

TEST_CASE("Dependency parse: less-than", "[semver]") {
  auto dep = Dependency::parse("storage < 3.0.0");
  CHECK(dep.type == "storage");
  CHECK(dep.op == Dependency::Op::lt);
  CHECK(dep.version == SemVer{3, 0, 0});
}

TEST_CASE("Dependency parse: caret", "[semver]") {
  auto dep = Dependency::parse("api ^1.2.3");
  CHECK(dep.type == "api");
  CHECK(dep.op == Dependency::Op::caret);
  CHECK(dep.version == SemVer{1, 2, 3});
}

TEST_CASE("Dependency parse: tilde", "[semver]") {
  auto dep = Dependency::parse("db ~2.1.0");
  CHECK(dep.type == "db");
  CHECK(dep.op == Dependency::Op::tilde);
  CHECK(dep.version == SemVer{2, 1, 0});
}

TEST_CASE("Dependency parse: equals", "[semver]") {
  auto dep = Dependency::parse("core == 1.0.0");
  CHECK(dep.type == "core");
  CHECK(dep.op == Dependency::Op::eq);
}

TEST_CASE("Dependency parse: not-equals", "[semver]") {
  auto dep = Dependency::parse("core != 1.0.0");
  CHECK(dep.type == "core");
  CHECK(dep.op == Dependency::Op::ne);
}

TEST_CASE("Dependency parse: extra spaces", "[semver]") {
  auto dep = Dependency::parse("  logger   >=   2.0  ");
  CHECK(dep.type == "logger");
  CHECK(dep.op == Dependency::Op::ge);
  CHECK(dep.version == SemVer{2, 0, 0});
}

TEST_CASE("Dependency parse: unknown operator throws", "[semver]") {
  CHECK_THROWS_AS(Dependency::parse("foo @@ 1.0"), std::runtime_error);
}

TEST_CASE("Dependency parse: missing version throws", "[semver]") {
  CHECK_THROWS_AS(Dependency::parse("foo >="), std::runtime_error);
}

// --- Constraint satisfaction ---

TEST_CASE("Dependency satisfied_by: any", "[semver]") {
  auto dep = Dependency::parse("logger");
  CHECK(dep.satisfied_by(SemVer{0, 0, 1}));
  CHECK(dep.satisfied_by(SemVer{99, 99, 99}));
}

TEST_CASE("Dependency satisfied_by: greater-equal", "[semver]") {
  auto dep = Dependency::parse("logger >= 2.0");
  CHECK_FALSE(dep.satisfied_by(SemVer{1, 9, 9}));
  CHECK(dep.satisfied_by(SemVer{2, 0, 0}));
  CHECK(dep.satisfied_by(SemVer{2, 1, 0}));
  CHECK(dep.satisfied_by(SemVer{3, 0, 0}));
}

TEST_CASE("Dependency satisfied_by: less-than", "[semver]") {
  auto dep = Dependency::parse("storage < 3.0.0");
  CHECK(dep.satisfied_by(SemVer{2, 9, 9}));
  CHECK_FALSE(dep.satisfied_by(SemVer{3, 0, 0}));
  CHECK_FALSE(dep.satisfied_by(SemVer{3, 0, 1}));
}

TEST_CASE("Dependency satisfied_by: caret (major > 0)", "[semver]") {
  auto dep = Dependency::parse("api ^1.2.3");
  CHECK_FALSE(dep.satisfied_by(SemVer{1, 2, 2}));  // below
  CHECK(dep.satisfied_by(SemVer{1, 2, 3}));         // exact
  CHECK(dep.satisfied_by(SemVer{1, 9, 0}));         // same major
  CHECK_FALSE(dep.satisfied_by(SemVer{2, 0, 0}));   // next major
}

TEST_CASE("Dependency satisfied_by: caret (0.x)", "[semver]") {
  auto dep = Dependency::parse("api ^0.2.3");
  CHECK_FALSE(dep.satisfied_by(SemVer{0, 2, 2}));
  CHECK(dep.satisfied_by(SemVer{0, 2, 3}));
  CHECK(dep.satisfied_by(SemVer{0, 2, 9}));
  CHECK_FALSE(dep.satisfied_by(SemVer{0, 3, 0}));
}

TEST_CASE("Dependency satisfied_by: caret (0.0.x)", "[semver]") {
  auto dep = Dependency::parse("api ^0.0.3");
  CHECK_FALSE(dep.satisfied_by(SemVer{0, 0, 2}));
  CHECK(dep.satisfied_by(SemVer{0, 0, 3}));
  CHECK_FALSE(dep.satisfied_by(SemVer{0, 0, 4}));
}

TEST_CASE("Dependency satisfied_by: tilde", "[semver]") {
  auto dep = Dependency::parse("db ~2.1.0");
  CHECK_FALSE(dep.satisfied_by(SemVer{2, 0, 9}));
  CHECK(dep.satisfied_by(SemVer{2, 1, 0}));
  CHECK(dep.satisfied_by(SemVer{2, 1, 5}));
  CHECK_FALSE(dep.satisfied_by(SemVer{2, 2, 0}));
}

TEST_CASE("Dependency satisfied_by: equals", "[semver]") {
  auto dep = Dependency::parse("core == 1.0.0");
  CHECK(dep.satisfied_by(SemVer{1, 0, 0}));
  CHECK_FALSE(dep.satisfied_by(SemVer{1, 0, 1}));
}

TEST_CASE("Dependency satisfied_by: not-equals", "[semver]") {
  auto dep = Dependency::parse("core != 1.0.0");
  CHECK_FALSE(dep.satisfied_by(SemVer{1, 0, 0}));
  CHECK(dep.satisfied_by(SemVer{1, 0, 1}));
  CHECK(dep.satisfied_by(SemVer{2, 0, 0}));
}

TEST_CASE("Dependency satisfied_by: less-equal", "[semver]") {
  auto dep = Dependency::parse("core <= 2.0.0");
  CHECK(dep.satisfied_by(SemVer{1, 0, 0}));
  CHECK(dep.satisfied_by(SemVer{2, 0, 0}));
  CHECK_FALSE(dep.satisfied_by(SemVer{2, 0, 1}));
}

TEST_CASE("Dependency satisfied_by: greater-than", "[semver]") {
  auto dep = Dependency::parse("core > 1.0.0");
  CHECK_FALSE(dep.satisfied_by(SemVer{1, 0, 0}));
  CHECK(dep.satisfied_by(SemVer{1, 0, 1}));
}
