#pragma once
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <fmt/format.h>


namespace {
  namespace fs = ::std::filesystem;
}

class ps_storage 
{
  static inline nlohmann::json m_jroot;
  static inline const char* const filepath = "persistent.json";

public:
  static ps_storage& get()
  {
    static ps_storage s;
    return s;
  }

  static nlohmann::json& jroot()
  {
    return get().m_jroot;
  }

  ps_storage(const ps_storage&) = delete;
  ps_storage& operator=(const ps_storage&) = delete;

  void load()
  {
    fs::path path(filepath);
    if (!fs::exists(path) || !fs::is_regular_file(path))
      return;

    try
    {
      std::ifstream ifile(path);
      if (ifile.is_open())
        ifile >> m_jroot;
    }
    catch (std::exception&)
    {
      // invalid format, let's make a backup if there isn't one already
      fs::path backup_path = path;
      backup_path /= path.filename().string() + "_backup";
      if (!fs::exists(backup_path))
        fs::rename(path, backup_path);
    }
  }

  void save()
  {
    fs::path path(filepath);
    if (fs::exists(path) && !fs::is_regular_file(path))
      return;

    std::ofstream ofile(filepath);
    if (ofile.is_open())
      ofile << m_jroot.dump(4);
  }

private:
  ps_storage()
  {
    // not the best idea
    load();
  }

  ~ps_storage()
  {
    // save on termination
    save();
  }
};

