#include <meios/io.h>
#include <meios/diagnostic/load_error.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include "meios/io/text_reader_operations.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <optional>
#include <filesystem>
#include <system_error>

namespace
{

class custom_error_category final : public std::error_category
{
public:
    const char *name() const noexcept override
    {
        return "operation-test";
    }

    std::string message(int value) const override
    {
        return std::to_string(value);
    }
};

const custom_error_category category;

class status_refusal final : public meios::detail::text_reader_operations
{
public:
    meios::detail::text_status_result status(const std::filesystem::path &) const noexcept override
    {
        return meios::unexpected<std::error_code>({37, category});
    }

    meios::detail::text_file_result open(const std::filesystem::path &) const noexcept override
    {
        return meios::unexpected<std::error_code>({99, category});
    }
};

class typed_sink final : public meios::log_sink
{
public:
    using meios::log_sink::log;

    typed_sink()
            : calls(0)
    {
    }

    void log(meios::level, meios::diagnostic_code, const meios::source_location &, const std::string &) override
    {
        ++calls;
    }

    int calls;
};

struct callable_recorder
{
    int &calls;

    void operator()(meios::level, const std::string &)
    {
        ++calls;
    }

    void operator()(meios::level, const meios::source_location &, const std::string &)
    {
        ++calls;
    }

    void operator()(meios::level, meios::diagnostic_code, const meios::source_location &, const std::string &)
    {
        ++calls;
    }
};

class file_guard
{
public:
    explicit file_guard(std::filesystem::path path)
            : m_path(std::move(path))
    {
    }

    ~file_guard()
    {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }

private:
    std::filesystem::path m_path;
};

}

TEST_CASE("operation names are stable presentation", "[operation_failure]")
{
    REQUIRE(meios::to_string(meios::operation_kind::status) == "status");
    REQUIRE(meios::to_string(meios::operation_kind::canonicalize) == "canonicalize");
    REQUIRE(meios::to_string(meios::operation_kind::open) == "open");
    REQUIRE(meios::to_string(meios::operation_kind::read) == "read");
    REQUIRE(meios::to_string(meios::operation_kind::close) == "close");
    REQUIRE(meios::to_string(meios::operation_kind::create) == "create");
    REQUIRE(meios::to_string(meios::operation_kind::permissions) == "permissions");
    REQUIRE(meios::to_string(meios::operation_kind::write) == "write");
    REQUIRE(meios::to_string(meios::operation_kind::publish) == "publish");
}

TEST_CASE("a status refusal retains its complete native cause", "[operation_failure]")
{
    status_refusal operations;
    const meios::text_read_result result = meios::detail::read_text_file("unreadable", operations);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == meios::text_read_failure_kind::status);
    REQUIRE(result.error().cause.operation == meios::operation_kind::status);
    REQUIRE(&result.error().cause.native.category() == &category);
    REQUIRE(result.error().cause.native.value() == 37);
}

TEST_CASE("a structured cause survives capture and load failure", "[operation_failure]")
{
    typed_sink inner;
    meios::capturing_log_sink capture{inner};
    meios::log_sink &seam = capture;
    const meios::operation_failure cause{meios::operation_kind::status, {37, category}};

    seam.log(meios::level::error, meios::diagnostic_code::cannot_open, {}, cause, "status failed");
    const std::optional<meios::captured_diagnostic> record = capture.first();
    REQUIRE(record.has_value());
    REQUIRE(record->cause.has_value());
    REQUIRE(record->cause->operation == cause.operation);
    REQUIRE(record->cause->native == cause.native);

    const meios::load_error error{{}, "status failed", meios::diagnostic_code::cannot_open, capture.records(), cause};
    REQUIRE(error.cause.has_value());
    REQUIRE(error.cause->operation == cause.operation);
    REQUIRE(error.cause->native == cause.native);
    REQUIRE(&error.cause->native.category() == &category);
    REQUIRE(error.cause->native.value() == 37);
    REQUIRE(inner.calls == 1);
}

TEST_CASE("legacy sinks and callable adapters receive one forwarded record", "[operation_failure]")
{
    const meios::operation_failure cause{meios::operation_kind::open, {41, category}};
    typed_sink sink;
    meios::log_sink &sink_seam = sink;
    sink_seam.log(meios::level::error, meios::diagnostic_code::cannot_open, {}, cause, "open failed");
    REQUIRE(sink.calls == 1);

    int calls = 0;
    meios::log_sink_f adapter{callable_recorder{calls}};
    meios::log_sink &adapter_seam = adapter;
    adapter_seam.log(meios::level::error, meios::diagnostic_code::cannot_open, {}, cause, "open failed");
    REQUIRE(calls == 1);
}

TEST_CASE("source lookup represents found absent and failed", "[operation_failure]")
{
    const meios::source_lookup_result found  = std::optional{meios::resolved_asset{"asset.urdf"}};
    const meios::source_lookup_result absent = std::optional<meios::resolved_asset>{};
    const meios::source_lookup_result failed = meios::unexpected<meios::operation_failure>({meios::operation_kind::canonicalize, {43, category}});

    REQUIRE(found->has_value());
    REQUIRE_FALSE(absent->has_value());
    REQUIRE_FALSE(failed.has_value());
    REQUIRE(&failed.error().native.category() == &category);
}

TEST_CASE("the production reader returns exact regular-file bytes", "[operation_failure]")
{
    const std::filesystem::path path = "operation-failure-readable.bin";
    const file_guard cleanup{path};
    const std::string bytes{"a\0b", 3};
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    REQUIRE(output.good());

    const meios::text_read_result result = meios::read_text_file(path);
    REQUIRE(result.has_value());
    REQUIRE(*result == bytes);
}
