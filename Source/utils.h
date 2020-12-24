#pragma once

#include <stdint.h>
#include <string>
#include <vector>

void replace_all_in_str(std::string& s, const std::string& from, const std::string& to);

std::string u64_to_cpp(uint64_t val);

std::vector<uintptr_t> sse2_strstr_masked(const unsigned char* s, size_t m, const unsigned char* needle, size_t n, const char* mask, size_t maxcnt = 0);
std::vector<uintptr_t> sse2_strstr(const unsigned char* s, size_t m, const unsigned char* needle, size_t n, size_t maxcnt = 0);
