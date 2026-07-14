#ifndef HPP_GUARD_MEIOS_COMPLETION_EMITTERS_H
#define HPP_GUARD_MEIOS_COMPLETION_EMITTERS_H

#include "meios/completion/completion_model.h"

#include <string>

namespace meios
{

// Each emitter serializes the same shell-neutral model into one shell's completion
// script. Adding a shell is one new function here; the model and table are untouched.
std::string emit_bash(const completion_model &model);

std::string emit_zsh(const completion_model &model);

std::string emit_fish(const completion_model &model);

}

#endif
