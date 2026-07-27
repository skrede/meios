#include "urdf_detail.h"

#include "meios/records/link.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <cmath>
#include <string>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

constexpr double relative_tolerance = 1e-9;

struct violation
{
    diagnostic_code  code;
    std::string_view what;
};

bool finite_tensor(const inertia<double> &t)
{
    return std::isfinite(t.ixx) && std::isfinite(t.ixy) && std::isfinite(t.ixz)
        && std::isfinite(t.iyy) && std::isfinite(t.iyz) && std::isfinite(t.izz);
}

bool wholly_zero(const inertial<double> &body)
{
    const inertia<double> &t = body.tensor;
    return body.mass == 0.0 && t.ixx == 0.0 && t.ixy == 0.0 && t.ixz == 0.0 && t.iyy == 0.0
        && t.iyz == 0.0 && t.izz == 0.0;
}

double scale_of(const inertia<double> &t)
{
    return std::max({ std::fabs(t.ixx), std::fabs(t.iyy), std::fabs(t.izz) });
}

double determinant(const inertia<double> &t)
{
    return t.ixx * (t.iyy * t.izz - t.iyz * t.iyz) - t.ixy * (t.ixy * t.izz - t.iyz * t.ixz)
        + t.ixz * (t.ixy * t.iyz - t.iyy * t.ixz);
}

bool diagonal_nonnegative(const inertia<double> &t, double eps)
{
    return t.ixx >= -eps && t.iyy >= -eps && t.izz >= -eps;
}

// Sylvester's criterion for positive semi-definiteness, which tests every principal minor
// rather than only the leading ones: the leading-minor form decides strict definiteness,
// and a rigid body may have a singular tensor, so it would refuse real links. The three
// minors of order one are the diagonal components, tested separately above.
bool minors_nonnegative(const inertia<double> &t, double scale, double eps)
{
    const double order2 = -eps * scale;
    return t.ixx * t.iyy - t.ixy * t.ixy >= order2 && t.ixx * t.izz - t.ixz * t.ixz >= order2
        && t.iyy * t.izz - t.iyz * t.iyz >= order2 && determinant(t) >= order2 * scale;
}

bool triangle_holds(const inertia<double> &t, double eps)
{
    return t.ixx + t.iyy - t.izz >= -eps && t.ixx + t.izz - t.iyy >= -eps
        && t.iyy + t.izz - t.ixx >= -eps;
}

std::optional<violation> mass_fault(double mass)
{
    if(!std::isfinite(mass))
        return violation{ diagnostic_code::invalid_mass, "declares a mass that is not finite" };
    if(mass < 0.0)
        return violation{ diagnostic_code::invalid_mass, "declares a negative mass" };
    return std::nullopt;
}

std::optional<violation> tensor_fault(const inertia<double> &t)
{
    if(!finite_tensor(t))
        return violation{ diagnostic_code::invalid_inertia,
                          "declares a tensor component that is not finite" };
    const double scale = scale_of(t);
    if(scale == 0.0)
        return violation{ diagnostic_code::invalid_inertia,
                          "declares a tensor whose diagonal is wholly zero while the tensor is not" };
    const double eps = relative_tolerance * scale;
    if(!diagonal_nonnegative(t, eps))
        return violation{ diagnostic_code::invalid_inertia,
                          "declares a negative moment of inertia" };
    if(!minors_nonnegative(t, scale, eps))
        return violation{ diagnostic_code::invalid_inertia,
                          "declares a tensor that is not positive semi-definite" };
    if(!triangle_holds(t, eps))
        return violation{ diagnostic_code::invalid_inertia,
                          "declares a tensor violating a triangle inequality" };
    return std::nullopt;
}

}

bool check_inertia(const inertial<double> &body, parse_context &ctx, const source_location &loc)
{
    // Finiteness is decided before any product is formed: components near the double
    // maximum overflow to an infinity inside the determinant, and the answer would then
    // be the overflow's rather than the document's.
    std::optional<violation> fault = mass_fault(body.mass);
    if(!fault && !wholly_zero(body))
        fault = tensor_fault(body.tensor);
    if(!fault)
        return true;
    report_drop(ctx, loc, fault->code,
                "<inertial> " + std::string(fault->what)
                    + ", so the inertial is dropped rather than read as a rigid body");
    return false;
}

}
