#include "../corpus_record.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{

const std::string records =
    "/corpus/a.urdf||core|"
    "|/corpus/b.xacro|ur_type=ur5e name=ur|python|/corpus/roots"
    "|/corpus/c.xacro|prefix=arm_|native|/corpus/roots"
    "|/corpus/d.xacro||lua|/corpus/roots";

}

TEST_CASE("a corpus record selects the evaluator its document needs", "[corpus]")
{
    const std::vector<corpus::document> docs = corpus::documents(records);
    REQUIRE(docs.size() == 4);
    CHECK(docs[0].backend == corpus::evaluator::core);
    CHECK(docs[1].backend == corpus::evaluator::python);
    CHECK(docs[2].backend == corpus::evaluator::native);
    CHECK(docs[3].backend == corpus::evaluator::unknown);
    CHECK(docs[3].spelling == "lua");
}

TEST_CASE("a corpus record carries its expansion arguments and package root", "[corpus]")
{
    const std::vector<corpus::document> docs = corpus::documents(records);
    REQUIRE(docs.size() == 4);
    CHECK(docs[0].path == "/corpus/a.urdf");
    CHECK(docs[0].args.empty());
    CHECK(docs[0].package_root.empty());
    REQUIRE(docs[1].args.size() == 2);
    CHECK(docs[1].args.at("ur_type") == "ur5e");
    CHECK(docs[1].args.at("name") == "ur");
    CHECK(docs[1].package_root == "/corpus/roots");
    CHECK(docs[2].args.at("prefix") == "arm_");
}

TEST_CASE("a record list that is not a whole number of records parses to nothing", "[corpus]")
{
    CHECK(corpus::documents("/corpus/a.urdf||core").empty());
    CHECK(corpus::documents("/corpus/a.urdf||core||/corpus/b.xacro||native").empty());
    CHECK(corpus::documents("").empty());
}
