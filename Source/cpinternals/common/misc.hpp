#pragma once
#include <inttypes.h>
#include <string>

namespace cp {

struct task_progress
{
	float normalized_value = 0.f;
	std::string comment;
};

// todo: deprecate
struct progress_t
{
	float value = 0.f;
	std::string comment;
};

} // namespace cp

