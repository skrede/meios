#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace
{

// Wide enough to absorb the round-off of a three-matrix product, far tighter than the
// difference any two competing composition orders produce on the discriminating row.
constexpr double tolerance = 1e-12;

struct reference
{
    std::vector<double> angles;
    std::vector<double> matrix;
    std::vector<double> quaternion;
};

std::vector<double> numbers_in(const std::string &field)
{
    std::istringstream in{ field };
    std::vector<double> out;
    for(double value = 0.0; in >> value;)
        out.push_back(value);
    return out;
}

reference row_of(const std::string &line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    for(std::size_t tab = line.find('\t'); tab != std::string::npos; tab = line.find('\t', start))
    {
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    fields.resize(4);
    return { numbers_in(fields[0]), numbers_in(fields[1]), numbers_in(fields[2]) };
}

std::vector<reference> load_rows()
{
    const std::filesystem::path path =
        std::filesystem::path{ MEIOS_GOLDEN_DIR } / "urdf" / "rpy_reference.cases";
    std::ifstream in(path);
    std::vector<reference> rows;
    std::string line;
    while(std::getline(in, line))
    {
        if(!line.empty() && line[0] != '#')
            rows.push_back(row_of(line));
    }
    return rows;
}

bool well_formed(const reference &r)
{
    return r.angles.size() == 3 && r.matrix.size() == 9 && r.quaternion.size() == 4;
}

std::vector<double> product(const std::vector<double> &a, const std::vector<double> &b)
{
    std::vector<double> out(9, 0.0);
    for(std::size_t row = 0; row < 3; ++row)
        for(std::size_t col = 0; col < 3; ++col)
            for(std::size_t k = 0; k < 3; ++k)
                out[row * 3 + col] += a[row * 3 + k] * b[k * 3 + col];
    return out;
}

std::vector<double> about_x(double angle)
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return { 1.0, 0.0, 0.0, 0.0, c, -s, 0.0, s, c };
}

std::vector<double> about_y(double angle)
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return { c, 0.0, s, 0.0, 1.0, 0.0, -s, 0.0, c };
}

std::vector<double> about_z(double angle)
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return { c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0 };
}

std::vector<double> fixed_axis(const std::vector<double> &angles)
{
    return product(about_z(angles[2]), product(about_y(angles[1]), about_x(angles[0])));
}

std::vector<double> moving_axis(const std::vector<double> &angles)
{
    return product(about_x(angles[0]), product(about_y(angles[1]), about_z(angles[2])));
}

std::vector<double> from_quaternion(const std::vector<double> &q)
{
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    return { 1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - w * z),       2.0 * (x * z + w * y),
             2.0 * (x * y + w * z),       1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - w * x),
             2.0 * (x * z - w * y),       2.0 * (y * z + w * x),       1.0 - 2.0 * (x * x + y * y) };
}

double norm_of(const std::vector<double> &q)
{
    return std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
}

double determinant(const std::vector<double> &m)
{
    return m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6])
        + m[2] * (m[3] * m[7] - m[4] * m[6]);
}

bool orthonormal(const std::vector<double> &m)
{
    for(std::size_t i = 0; i < 3; ++i)
        for(std::size_t j = 0; j < 3; ++j)
        {
            double dot = 0.0;
            for(std::size_t k = 0; k < 3; ++k)
                dot += m[i * 3 + k] * m[j * 3 + k];
            if(std::fabs(dot - (i == j ? 1.0 : 0.0)) > tolerance)
                return false;
        }
    return true;
}

bool agree(const std::vector<double> &a, const std::vector<double> &b)
{
    for(std::size_t at = 0; at < 9; ++at)
        if(std::fabs(a[at] - b[at]) > tolerance)
            return false;
    return true;
}

}

TEST_CASE("the rotation reference parses into rows of the shape its header declares", "[urdf][frames]")
{
    const std::vector<reference> rows = load_rows();
    REQUIRE(rows.size() >= 5);
    for(const reference &r : rows)
        REQUIRE(well_formed(r));
}

TEST_CASE("every rotation the reference publishes is a rotation", "[urdf][frames]")
{
    for(const reference &r : load_rows())
    {
        REQUIRE(well_formed(r));
        REQUIRE(std::fabs(norm_of(r.quaternion) - 1.0) <= tolerance);
        REQUIRE(orthonormal(r.matrix));
        REQUIRE(std::fabs(determinant(r.matrix) - 1.0) <= tolerance);
    }
}

TEST_CASE("each row's matrix and quaternion describe the same rotation", "[urdf][frames]")
{
    for(const reference &r : load_rows())
    {
        REQUIRE(well_formed(r));
        REQUIRE(agree(r.matrix, from_quaternion(r.quaternion)));
    }
}

TEST_CASE("every row is the fixed-axis composition the profile publishes", "[urdf][frames]")
{
    for(const reference &r : load_rows())
    {
        REQUIRE(well_formed(r));
        REQUIRE(agree(r.matrix, fixed_axis(r.angles)));
    }
}

TEST_CASE("the reference discriminates the composition order", "[urdf][frames]")
{
    const std::vector<reference> rows = load_rows();
    REQUIRE_FALSE(rows.empty());
    bool discriminating = false;
    for(const reference &r : rows)
    {
        REQUIRE(well_formed(r));
        discriminating = discriminating || !agree(r.matrix, moving_axis(r.angles));
    }
    REQUIRE(discriminating);
}
