#include "utils.h"

#include <string>
#include <sstream>
#include <iomanip>

void replace_all_in_str(std::string& s, const std::string& from, const std::string& to)
{
  size_t start_pos = 0;
  while((start_pos = s.find(from, start_pos)) != std::string::npos) {
    s.replace(start_pos, from.length(), to);
    start_pos += to.length(); // handles case where 'to' is a substring of 'from'
  }
}

std::string u64_to_cpp(uint64_t val)
{
  std::stringstream ss;
  ss << "0x" << std::setfill('0') << std::setw(16) 
    << std::hex << std::uppercase << val;
  return ss.str();
}

