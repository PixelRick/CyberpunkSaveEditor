#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace cp::os {

// windows only atm
using error_type = uint32_t;

std::optional<std::filesystem::path> get_cp_executable_path();

std::string last_error_string();

std::string format_error(error_type id);

} // namespace cp::os

