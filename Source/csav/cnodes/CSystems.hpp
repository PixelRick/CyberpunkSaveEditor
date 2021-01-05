#pragma once
#include <inttypes.h>

#include <cpinternals/cpnames.hpp>
#include <csav/node.hpp>
#include <csav/serializers.hpp>
#include <csav/csystem/CSystem.hpp>

struct CSystemGeneric
{
  std::shared_ptr<const node_t> m_raw; // temporary
  std::string node_name;
  CSystem m_sys;

public:
  CSystemGeneric() = default;


  const CSystem& system() const { return m_sys; }
        CSystem& system()       { return m_sys; }


  bool from_node(const std::shared_ptr<const node_t>& node, const csav_version& version)
  {
    if (!node)
      return false;

    m_raw = node;
    node_name = node->name();

    node_reader reader(node, version);

    if (!m_sys.serialize_in(reader))
        return false;

    return reader.at_end();
  }

  std::shared_ptr<const node_t> to_node(const csav_version& version)
  {
    node_writer writer(version);
    
    if (!m_sys.serialize_out(writer))
      return nullptr;

    return writer.finalize(node_name);
  }
};


struct CPSData
{
  std::shared_ptr<const node_t> m_raw; // temporary
  std::string node_name;
  CSystem m_sys;
  std::vector<CName> trailing_names;

public:
  CPSData() = default;


  const CSystem& system() const { return m_sys; }
        CSystem& system()       { return m_sys; }


  bool from_node(const std::shared_ptr<const node_t>& node, const csav_version& version)
  {
    if (!node)
      return false;

    m_raw = node;
    node_name = node->name();

    node_reader reader(node, version);

    // todo: catch exception at upper level to display error
    if (!m_sys.serialize_in(reader))
      return false;

    uint32_t cnt = 0;
    reader >> cbytes_ref(cnt);
    trailing_names.resize(cnt);
    reader.read((char*)trailing_names.data(), cnt * sizeof(CName));

    return reader.at_end();
  }

  std::shared_ptr<const node_t> to_node(const csav_version& version)
  {
    node_writer writer(version);

    try
    {
      if (!m_sys.serialize_out(writer))
        return nullptr;

      uint32_t cnt = (uint32_t)trailing_names.size();
      writer << cbytes_ref(cnt);
      writer.write((char*)trailing_names.data(), cnt * sizeof(CName));
    }
    catch (std::exception&)
    {
      return nullptr;
    }

    return writer.finalize(node_name);
  }
};


