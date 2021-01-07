#pragma once
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <fmt/format.h>


namespace {
  namespace fs = ::std::filesystem;
}

class ps_json_storage 
{
  nlohmann::json m_jroot;
  static inline const char* const s_filepath = "persistent.json";

private:
  ps_json_storage()
  {
    // not the best idea
    load();
  }

  ~ps_json_storage()
  {
    // save on termination
    save();
  }

public:
  ps_json_storage(const ps_json_storage&) = delete;
  ps_json_storage& operator=(const ps_json_storage&) = delete;

  static ps_json_storage& get()
  {
    static ps_json_storage s;
    return s;
  }

  nlohmann::json& jroot()
  {
    return m_jroot;
  }

  void load()
  {
    fs::path path = s_filepath;
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
    fs::path path = s_filepath;
    if (fs::exists(path) && !fs::is_regular_file(path))
      return;

    std::ofstream ofile(path);
    if (ofile.is_open())
      ofile << m_jroot.dump(4);
  }
};

