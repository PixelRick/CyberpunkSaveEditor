#include "init.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "core.hpp"
#include "ctypes.hpp"

namespace redx {

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

bool load_names_from_txt(std::filesystem::path path, std::vector<gname>& out, bool optional = false)
{
  auto relpath = std::filesystem::relative(path);

  std::ifstream ifs(path);
  if (ifs.is_open())
  {
    try
    {
      std::string line;
      while (std::getline(ifs, line))
      {
          if (line.size())
          {
            out.emplace_back(line);
          }
      }
    }
    catch (std::exception&)
    {
      MessageBox(0, fmt::format(L"{} has unexpected content", relpath.wstring()).c_str(), L"corrupt resource file", 0);
      return false;
    }
  }
  else
  {
    if (!optional)
      MessageBox(0, fmt::format(L"{} is missing", relpath.wstring()).c_str(), L"missing resource file", 0);
    return false;
  }

  return true;
}

// TODO: rename this an add progress param
bool init_redx(bool with_archive_names)
{
  {
    std::vector<gname> names;
    load_from_json("./db/TweakDBIDs.json", names);
    TweakDBID_resolver::get().feed(names);
  }

  {
    std::vector<gname> names;
    load_names_from_txt("./db/internal_names.txt", names);

    if (0 && with_archive_names)
    {
      load_names_from_txt("./db/archive_names.txt", names);

      if (std::filesystem::exists("./db/custom_archive_names.txt"))
      {
        load_names_from_txt("./db/custom_archive_names.txt", names);
      }
    }

    CName_resolver::get().feed(names);
  }

  {
    std::vector<gname> names;
    load_from_json("./db/CFacts.json", names);
    CFact_resolver::get().feed(names);
  }

  {
    load_from_json("./db/CEnums.json", CEnum_resolver::get());
  }

  return true;
}

} // namespace redx

