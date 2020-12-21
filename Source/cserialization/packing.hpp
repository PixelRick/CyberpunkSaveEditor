#pragma once
#include <cstdint>
#include <iostream>

int64_t read_packed_int(std::istream& is);
void write_packed_int(std::ostream& os, int64_t value);

std::string read_str(std::istream& is);
void write_str(std::ostream& os, const std::string& s);

