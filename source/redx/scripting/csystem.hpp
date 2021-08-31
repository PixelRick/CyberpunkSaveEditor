#pragma once
#include <iostream>
#include <list>
#include <array>
#include <exception>

#include "redx/common.hpp"
#include "redx/ctypes.hpp"
#include "redx/csav/node.hpp"
#include "redx/csav/serializers.hpp"
#include "CStringPool.hpp"
#include "cobject.hpp"
#include "redx/io/stdstream_wrapper.hpp"

enum class ESystemKind : uint8_t
{
  DropPointSystem,
  GameplaySettingsSystem,
  EquipmentSystem,
  PlayerDevelopmentSystem,
  BraindanceSystem,
  UIScriptableSystem,
  PreventionSystem,
  FocusCluesSystem,
  FastTravelSystem,
  CityLightSystem,
  FirstEquipSystem,
  SubCharacterSystem,
  SecSystemDebugger,
  MarketSystem,
  CraftingSystem,
  DataTrackingSystem,
};



class CSystem
{
private:
  struct header_t
  {
    uint16_t uk1                  = 0;
    uint16_t uk2                  = 0;
    uint32_t cnames_cnt           = 0;
    uint32_t strpool_descs_offset                  = 0;
    uint32_t strpool_data_offset  = 0;
    uint32_t obj_descs_offset     = 0;
    // tricky: obj offsets are relative to the strpool base
    uint32_t objdata_offset       = 0; 
  };

  struct obj_desc_t
  {
    obj_desc_t() = default;

    obj_desc_t(uint32_t name_idx, uint32_t data_offset)
      : name_idx(name_idx), data_offset(data_offset) {}

    uint32_t name_idx     = 0;
    uint32_t data_offset  = 0;// relative to the end of the header in stream
  };

private:
  header_t m_header;
  std::vector<CName> m_subsys_names;

  // those are root objects, not all the serialized ones
  std::vector<CObjectSPtr> m_objects;
  // these are backed-up handle-objects for reserialization
  std::vector<CObjectSPtr> m_handle_objects;

  // for now let's not rebuild the strpool from scratch
  // since we don't handle all types..
  CSystemSerCtx m_serctx;

public:
  CSystem() = default;
  ~CSystem() = default;

public:
  const std::vector<CName>& subsys_names() const { return m_subsys_names; }
        std::vector<CName>& subsys_names()       { return m_subsys_names; }

  const std::vector<CObjectSPtr>& objects() const { return m_objects; }
        std::vector<CObjectSPtr>& objects()       { return m_objects; }

public:

  bool serialize_in(std::istream& reader)
  {
    uint32_t blob_size = 0;
    reader >> cbytes_ref(blob_size);

    return serialize_in_sized(reader, blob_size, true);
  }

  bool serialize_in_sized(std::istream& reader, uint32_t blob_size, bool do_cnames=false)
  {
    m_subsys_names.clear();
    m_objects.clear();
    m_handle_objects.clear();

    // let's get our header start position
    auto blob_spos = reader.tellg();

    reader >> cbytes_ref(m_header);

    // check header
    if (m_header.obj_descs_offset < m_header.strpool_data_offset)
      return false;
    if (m_header.objdata_offset < m_header.obj_descs_offset)
      return false;

    m_subsys_names.clear();
    if (m_header.cnames_cnt > 1 && do_cnames)
    {

      uint32_t cnames_cnt = 0;
      reader >> cbytes_ref(cnames_cnt);
      
      if (cnames_cnt != m_header.cnames_cnt)
        return false;

      static_assert(sizeof(CName) == 8);

      m_subsys_names.resize(cnames_cnt);
      reader.read((char*)m_subsys_names.data(), m_header.cnames_cnt * sizeof(CName));
    }

    // end of header
    const size_t base_offset = (reader.tellg() - blob_spos);
    if (base_offset + m_header.objdata_offset > blob_size)
      return false;

    // first let's read the string pool

    const uint32_t strpool_descs_size = m_header.strpool_data_offset;
    const uint32_t strpool_data_size = m_header.obj_descs_offset - strpool_descs_size;


    auto blob = std::make_unique<char[]>(blob_size - base_offset);
    reader.read(blob.get(), blob_size - base_offset);
    //reader.seekg((size_t)blob_spos + base_offset);

    //CStringPool strpool;
    redx::cnameset& strpool = m_serctx.strpool;
    if (!strpool.read_in(blob.get(), 0, strpool_descs_size, strpool_data_size))
      return false;

    // now let's read objects

    reader.seekg((size_t)blob_spos + base_offset + m_header.obj_descs_offset);

    // we don't have the impl for all props
    // so we'll use the assumption that everything is serialized in
    // order and use the offset of next item as end of blob

    const size_t obj_descs_offset = m_header.obj_descs_offset;
    const size_t obj_descs_size = m_header.objdata_offset - obj_descs_offset;
    if (obj_descs_size % sizeof(obj_desc_t) != 0)
      return false;

    const size_t obj_descs_cnt = obj_descs_size / sizeof(obj_desc_t);
    if (obj_descs_cnt == 0)
      return m_header.objdata_offset + base_offset == blob_size; // could be empty

    std::vector<obj_desc_t> obj_descs(obj_descs_cnt);
    reader.read((char*)obj_descs.data(), obj_descs_size);

    // read objdata
    const size_t objdata_size = blob_size - (base_offset + m_header.objdata_offset); 

    if (base_offset + m_header.objdata_offset != (reader.tellg() - blob_spos))
      return false;

    std::vector<char> objdata(objdata_size);
    reader.read((char*)objdata.data(), objdata_size);

    if (blob_size != (reader.tellg() - blob_spos))
      return false;

    // prepare default initialized objects
    m_serctx.m_objects.clear();
    m_serctx.m_objects.reserve(obj_descs.size());
    for (auto it = obj_descs.begin(); it != obj_descs.end(); ++it)
    {
      const auto& desc = *it;

      // check desc is valid
      if (desc.data_offset < m_header.objdata_offset)
        return false;

      auto obj_ctypename = strpool.at(desc.name_idx).gstr();
      auto new_obj = std::make_shared<CObject>(obj_ctypename, true); // todo: static create method
      m_serctx.m_objects.push_back(new_obj);
    }

    // here the offsets relative to base_offset are converted to offsets relative to objdata
    size_t next_obj_offset = objdata_size;
    auto obj_it = m_serctx.m_objects.rbegin();
    for (auto it = obj_descs.rbegin(); it != obj_descs.rend(); ++it, ++obj_it)
    {
      const auto& desc = *it;

      const size_t offset = desc.data_offset - m_header.objdata_offset;
      if (offset > next_obj_offset)
        throw std::logic_error("CSystem: false assumption #2. please open an issue.");

      std::span<char> objblob((char*)objdata.data() + offset, next_obj_offset - offset);
      if (!(*obj_it)->serialize_in(objblob, m_serctx))
        return false;

      next_obj_offset = offset;
    }

    const auto& serobjs = m_serctx.m_objects;
    size_t root_obj_cnt = m_subsys_names.size();
    if (root_obj_cnt == 0)
      root_obj_cnt = 1;
    m_objects.assign(serobjs.begin(), serobjs.begin() + root_obj_cnt);
    m_handle_objects.assign(serobjs.begin() + root_obj_cnt, serobjs.end()); // backup handle-objects

    return true;
  }

  bool serialize_out(std::ostream& writer) const
  {
    // let's not do too many copies

    auto start_spos = writer.tellp();
    uint32_t blob_size = 0; // we don't know it yet
    
    writer << cbytes_ref(blob_size); 
    writer << cbytes_ref(m_header); 

    header_t new_header = m_header;

    const uint32_t cnames_cnt = (uint32_t)m_subsys_names.size();
    new_header.cnames_cnt = 1;
    if (cnames_cnt)
    {
      new_header.cnames_cnt = cnames_cnt;
      writer << cbytes_ref(cnames_cnt);
      writer.write((char*)m_subsys_names.data(), cnames_cnt * sizeof(CName));
    }

    stdstream_wrapper stw(writer);
    uint32_t base_spos = (uint32_t)stw.tell();

    // i see no choice but to build objects first to populate the pool
    // the game probably actually uses this pool so they don't have
    // to do that, but it is also not leaned at all.

    //CStringPool strpool;
    CSystemSerCtx& serctx = const_cast<CSystemSerCtx&>(m_serctx);
    // here we can't really remove handle-objects because there might be hidden handles in unsupported types
    serctx.m_objects.assign(m_objects.begin(), m_objects.end());
    serctx.m_objects.insert(serctx.m_objects.end(), m_handle_objects.begin(), m_handle_objects.end());
    serctx.rebuild_handlemap();

    std::ostringstream ss;
    std::vector<obj_desc_t> obj_descs;
    obj_descs.reserve(serctx.m_objects.size()); // ends up higher in the presence of handles

    // serctx.m_objects is extended during object serialization (handles)
    for (size_t i = 0; i < serctx.m_objects.size(); ++i)
    {
      auto& obj = serctx.m_objects[i];
      const uint32_t tmp_offset = (uint32_t)ss.tellp();
      const uint16_t name_idx = serctx.strpool.insert(obj->ctypename());
      obj_descs.emplace_back(name_idx, tmp_offset);
      if (!obj->serialize_out(ss, serctx))
        return false;
    }


    // time to write strpool
    uint32_t strpool_data_size = 0;
    new_header.strpool_descs_offset = 0;
    serctx.strpool.serialize_out(stw, base_spos, new_header.strpool_data_offset, strpool_data_size);
    
    if (stw.has_error())
      return false;

    new_header.obj_descs_offset = new_header.strpool_data_offset + strpool_data_size;

    // reoffset offsets
    const uint32_t obj_descs_size = (uint32_t)(obj_descs.size() * sizeof(obj_desc_t));
    const uint32_t objdata_offset = new_header.obj_descs_offset + obj_descs_size;

    new_header.objdata_offset = objdata_offset;

    // reoffset offsets
    for (auto& desc : obj_descs)
      desc.data_offset += objdata_offset;

    // write obj descs + data
    writer.write((char*)obj_descs.data(), obj_descs_size);
    auto objdata = ss.str(); // not optimal but didn't want to depend on boost and no time to dev a mem stream for now
    writer.write(objdata.data(), objdata.size());
    auto end_spos = writer.tellp();

    // at this point data should be correct except blob_size and header
    // so let's rewrite them

    writer.seekp(start_spos);
    blob_size = (uint32_t)(end_spos - start_spos - 4);
    writer << cbytes_ref(blob_size); 
    writer << cbytes_ref(new_header); 

    // return to end of data
    writer.seekp(end_spos);

    return true;
  }

  friend std::istream& operator>>(std::istream& reader, CSystem& sys)
  {
    if (!sys.serialize_in(reader))
      reader.setstate(std::ios_base::badbit);
    return reader;
  }

  friend std::ostream& operator<<(std::ostream& writer, CSystem& sys)
  {
    sys.serialize_out(writer);
    return writer;
  }
};


