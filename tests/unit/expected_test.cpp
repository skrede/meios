#include <meios/expected.h>

#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>
#include <system_error>

TEST_CASE("a value-carrying expected reports success and yields the value", "[model][expected]")
{
    meios::expected<int, int> e{5};

    REQUIRE(e.has_value());
    REQUIRE(static_cast<bool>(e));
    REQUIRE(*e == 5);
    REQUIRE(e.value() == 5);
}

TEST_CASE("an unexpected-constructed expected reports failure and yields the error", "[model][expected]")
{
    meios::expected<int, int> e{meios::unexpected<int>{42}};

    REQUIRE_FALSE(e.has_value());
    REQUIRE_FALSE(static_cast<bool>(e));
    REQUIRE(e.error() == 42);
}

TEST_CASE("an expected builds its error in place from the unexpect tag", "[model][expected]")
{
    meios::expected<int, int> e{meios::unexpect, 7};

    REQUIRE_FALSE(e.has_value());
    REQUIRE(e.error() == 7);
}

TEST_CASE("an expected moves a move-only value out through the rvalue operator*", "[model][expected]")
{
    meios::expected<std::unique_ptr<int>, int> e{std::make_unique<int>(9)};

    REQUIRE(e.has_value());

    std::unique_ptr<int> moved = *std::move(e);
    REQUIRE(moved != nullptr);
    REQUIRE(*moved == 9);
}

TEST_CASE("expected<void, E> default-constructs as a value and carries an error", "[model][expected]")
{
    meios::expected<void, int> ok{};
    REQUIRE(ok.has_value());
    REQUIRE(static_cast<bool>(ok));

    meios::expected<void, int> bad{meios::unexpected<int>{3}};
    REQUIRE_FALSE(bad.has_value());
    REQUIRE(bad.error() == 3);
}

TEST_CASE("expected<void, operation_failure> keeps the operation and the native code", "[model][expected]")
{
    const meios::operation_failure cause{meios::operation_kind::permissions, make_error_code(std::errc::permission_denied)};

    const meios::expected<void, meios::operation_failure> ok{};
    REQUIRE(ok.has_value());

    const meios::expected<void, meios::operation_failure> bad{meios::unexpected<meios::operation_failure>{cause}};
    REQUIRE_FALSE(bad.has_value());
    REQUIRE(bad.error().operation == meios::operation_kind::permissions);
    REQUIRE(bad.error().native == cause.native);
}
