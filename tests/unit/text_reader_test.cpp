#include "acquisition_script.h"

#include "meios/io/text_reader_operations.h"

#include <meios/io/text_reader.h>

#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <system_error>

namespace
{

meios::text_read_result run(acquisition_test::reader_script &script)
{
    acquisition_test::scripted_operations operations{script};
    return meios::detail::read_text_file("scripted", operations);
}

void require_failure(const meios::text_read_result &result, meios::text_read_failure_kind kind, meios::operation_kind operation, int value)
{
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == kind);
    REQUIRE(result.error().cause.operation == operation);
    REQUIRE(result.error().cause.native.value() == value);
}

}

TEST_CASE("scripted operation refusals stop at their failing boundary", "[text_reader]")
{
    acquisition_test::reader_script status;
    status.status_error = {11, acquisition_test::category};
    require_failure(run(status), meios::text_read_failure_kind::status, meios::operation_kind::status, 11);
    REQUIRE(status.calls == std::vector<std::string>{"status"});

    acquisition_test::reader_script open;
    open.open_error = {13, acquisition_test::category};
    require_failure(run(open), meios::text_read_failure_kind::open, meios::operation_kind::open, 13);
    REQUIRE(open.calls == std::vector<std::string>{"status", "open"});

    acquisition_test::reader_script read;
    read.read_error = {17, acquisition_test::category};
    require_failure(run(read), meios::text_read_failure_kind::read, meios::operation_kind::read, 17);
    REQUIRE(read.calls == std::vector<std::string>{"status", "open", "read", "error", "close"});
    REQUIRE(read.closes == 1);
}

TEST_CASE("zero-error and over-transfer backend failures are deterministic", "[text_reader]")
{
    acquisition_test::reader_script zero_read;
    zero_read.fail_read                = true;
    const meios::text_read_result read = run(zero_read);
    require_failure(read, meios::text_read_failure_kind::read, meios::operation_kind::read, int(std::errc::io_error));
    REQUIRE(&read.error().cause.native.category() == &std::generic_category());

    acquisition_test::reader_script zero_close;
    zero_close.fail_close               = true;
    const meios::text_read_result close = run(zero_close);
    require_failure(close, meios::text_read_failure_kind::close, meios::operation_kind::close, int(std::errc::io_error));

    acquisition_test::reader_script over;
    over.over_transfer                        = 1;
    const meios::text_read_result transferred = run(over);
    require_failure(transferred, meios::text_read_failure_kind::read, meios::operation_kind::read, int(std::errc::io_error));
    REQUIRE(over.closes == 1);
}

TEST_CASE("empty binary and multi-chunk content remain exact", "[text_reader]")
{
    acquisition_test::reader_script empty;
    empty.content.clear();
    const meios::text_read_result empty_result = run(empty);
    REQUIRE(empty_result.has_value());
    REQUIRE(empty_result->empty());

    acquisition_test::reader_script binary;
    binary.chunks                        = {"one", std::string{"t\0wo", 4}, "three"};
    const meios::text_read_result result = run(binary);
    REQUIRE(result.has_value());
    REQUIRE(*result == std::string{"onet\0wothree", 12});
    REQUIRE(binary.request == 65536);
    REQUIRE(binary.closes == 1);
}
