#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace cp::windowz {

std::optional<std::filesystem::path> get_cp_executable_path();

std::string get_last_error();

std::string format_error(uint32_t id);

} // namespace cp::windowz

