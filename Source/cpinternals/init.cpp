#include "init.hpp"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include "common.hpp"

namespace cp {

template <typename T>
bool load_from_json(std::filesystem::path path, T& out)
{
  auto relpath = std::filesystem::relative(path);

  std::ifstream ifs(path);
  if (ifs.is_open())
  {
    try
    {
      nlohmann::json db;
      ifs >> db;
      db.get_to(out);
    }
    catch (std::exception&)
    {
      MessageBox(0, fmt::format(L"{} has unexpected content", relpath.wstring()).c_str(), L"corrupt resource file", 0);
      return false;
    }
  }
  else
  {
    MessageBox(0, fmt::format(L"{} is missing", relpath.wstring()).c_str(), L"missing resource file", 0);
    return false;
  }

  return true;
}


bool init_cpinternals()
{
  {
    std::vector<gname> names;
    load_from_json("./db/TweakDBIDs.json", names);
    TweakDBID_resolver::get().feed(names);
  }

  {
    std::vector<gname> names;
    load_from_json("./db/CNames.json", names);
    CName_resolver::get().feed(names);
  }

  return true;
}

}

