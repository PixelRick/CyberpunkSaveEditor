#pragma once
#include <iostream>
#include <list>
#include <exception>

#include "cpinternals/common.hpp"
#include "cpinternals/ctypes.hpp"
#include "cpinternals/csav/node.hpp"
#include "cpinternals/csav/serializers.hpp"

struct cetr_uk_thing5
{
  std::string uk0;
  std::string uk1;
  std::string uk2;

  friend node_reader& operator>>(node_reader& reader, cetr_uk_thing5& x)
  {
    reader >> cp_plstring_ref(x.uk0);
    reader >> cp_plstring_ref(x.uk1);
    reader >> cp_plstring_ref(x.uk2);
    return reader;
  }

  friend node_writer& operator<<(node_writer& writer, const cetr_uk_thing5& x)
  {
    writer << cp_plstring_ref(x.uk0);
    writer << cp_plstring_ref(x.uk1);
    writer << cp_plstring_ref(x.uk2);
    return writer;
  }
};

struct cetr_uk_thing4
{
  std::string uk0;
  std::string uk1;
  uint32_t uk2 = 0;
  uint32_t uk3 = 0;

  friend node_reader& operator>>(node_reader& reader, cetr_uk_thing4& x)
  {
    reader >> cp_plstring_ref(x.uk0);
    reader >> cp_plstring_ref(x.uk1);
    // if (vmajor >= 168)
    reader >> cbytes_ref(x.uk2);
    reader >> cbytes_ref(x.uk3);
    return reader;
  }

  friend node_writer& operator<<(node_writer& writer, const cetr_uk_thing4& x)
  {
    writer << cp_plstring_ref(x.uk0);
    writer << cp_plstring_ref(x.uk1);
    // if (vmajor >= 168)
    writer << cbytes_ref(x.uk2);
    writer << cbytes_ref(x.uk3);
    return writer;
  }
};

// the real struct size is 0x28
struct cetr_uk_thing3
{
  CName cn = {};
  std::string uk0;
  std::string uk1;
  uint32_t uk2 = 0;
  uint32_t uk3 = 0;

  friend node_reader& operator>>(node_reader& reader, cetr_uk_thing3& x)
  {
    if (reader.version().v3 < 195)
    {
      std::string s;
      reader >> cp_plstring_ref(s);
      x.cn = CName(s);
      CName_resolver::get().resolve(x.cn);
    }
    else
    {
      reader >> cbytes_ref(x.cn.as_u64);
    }
    reader >> cp_plstring_ref(x.uk0);
    // if (v1 >= 168)
    reader >> cp_plstring_ref(x.uk1);
    reader >> cbytes_ref(x.uk2);
    reader >> cbytes_ref(x.uk3);
    return reader;
  }

  friend node_writer& operator<<(node_writer& writer, const cetr_uk_thing3& x)
  {
    if (writer.version().v3 < 195)
    {
      const auto& resolver = CName_resolver::get();
      if (!resolver.is_registered(x.cn))
      {
        writer.setstate(std::ios::badbit);
        return writer;
      }
      auto cns = std::string(resolver.resolve(x.cn));
      writer << cp_plstring_ref(cns);
    }
    else
    {
      writer << cbytes_ref(x.cn.as_u64);
    }
    writer << cp_plstring_ref(x.uk0);
    // if (v1 >= 168)
    writer << cp_plstring_ref(x.uk1);
    writer << cbytes_ref(x.uk2);
    writer << cbytes_ref(x.uk3);
    return writer;
  }
};

struct cetr_uk_thing2
{
  std::string uks;
  std::list<cetr_uk_thing3> vuk3;
  std::list<cetr_uk_thing4> vuk4;

  friend node_reader& operator>>(node_reader& reader, cetr_uk_thing2& x)
  {
    reader >> cp_plstring_ref(x.uks);
    // if (v1 < 168) { .. }
    {
      uint32_t cnt = 0;
      reader >> cbytes_ref(cnt);
      x.vuk3.resize(cnt);
      for (auto& y : x.vuk3)
        reader >> y;
    }
    {
      uint32_t cnt = 0;
      reader >> cbytes_ref(cnt);
      x.vuk4.resize(cnt);
      for (auto& y : x.vuk4)
        reader >> y;
    }
    return reader;
  }

  friend node_writer& operator<<(node_writer& writer, const cetr_uk_thing2& x)
  {
    writer << cp_plstring_ref(x.uks);
    // if (v1 < 168) { .. }
    {
      uint32_t cnt = (uint32_t)x.vuk3.size();
      writer << cbytes_ref(cnt);
      for (auto& y : x.vuk3)
        writer << y;
    }
    {
      uint32_t cnt = (uint32_t)x.vuk4.size();
      writer << cbytes_ref(cnt);
      for (auto& y : x.vuk4)
        writer << y;
    }
    return writer;
  }
};

struct cetr_uk_thing1
{
  std::list<cetr_uk_thing2> vuk2;

  friend node_reader& operator>>(node_reader& reader, cetr_uk_thing1& x)
  {
    uint32_t cnt = 0;
    reader >> cbytes_ref(cnt);
    x.vuk2.resize(cnt);
    for (auto& y : x.vuk2)
      reader >> y;
    return reader;
  }

  friend node_writer& operator<<(node_writer& writer, const cetr_uk_thing1& x)
  {
    uint32_t cnt = (uint32_t)x.vuk2.size();
    writer << cbytes_ref(cnt);
    for (auto& y : x.vuk2)
      writer << y;
    return writer;
  }
};


struct CCharacterCustomization
  : public node_serializable
{
  uint8_t data_exists = 0;

  uint32_t uk0 = 0;
  uint32_t uk1 = 0;
  uint8_t uk2 = 0;
  uint8_t uk3 = 0;

  cetr_uk_thing1 ukt0;
  cetr_uk_thing1 ukt1;
  cetr_uk_thing1 ukt2;

  std::list<cetr_uk_thing5> ukt5;

  std::list<std::string> uk6s;

  std::string node_name() const override { return "CharacetrCustomization_Appearances"; }

  bool from_node_impl(const std::shared_ptr<const node_t>& node, const csav_version& version) override
  {
    if (!node)
      return false;

    node_reader reader(node, version);

    try
    {
      data_exists = false;
      reader >> cbytes_ref(data_exists);

      reader >> cbytes_ref(uk0);

      if (data_exists)
      {
        reader >> cbytes_ref(uk1);
        reader >> cbytes_ref(uk2);
        reader >> cbytes_ref(uk3);
        reader >> ukt0;
        reader >> ukt1;
        reader >> ukt2;

        uint32_t cnt = 0;
        reader >> cbytes_ref(cnt);
        ukt5.resize(cnt);
        for (auto& y : ukt5)
          reader >> y;

        int64_t uk6cnt = 0;
        if (version.v1 > 171)
          reader >> cp_packedint_ref(uk6cnt);
        uk6s.resize(uk6cnt);
        for (auto& y : uk6s)
          reader >> cp_plstring_ref(y);
      }
    }
    catch (std::ios::failure&)
    {
      return false;
    }

    return reader.at_end();
  }

  std::shared_ptr<const node_t> to_node_impl(const csav_version& version) const override
  {
    node_writer writer(version);

    try
    {
      writer << cbytes_ref(data_exists);

      writer << cbytes_ref(uk0);

      if (data_exists)
      {
        writer << cbytes_ref(uk1);
        writer << cbytes_ref(uk2);
        writer << cbytes_ref(uk3);
        writer << ukt0;
        writer << ukt1;
        writer << ukt2;

        uint32_t cnt = (uint32_t)ukt5.size();
        writer << cbytes_ref(cnt);
        for (auto& y : ukt5)
          writer << y;

        if (version.v1 > 171)
        {
          int64_t uk6cnt = uk6s.size();
          writer << cp_packedint_ref(uk6cnt);
          for (auto& y : uk6s)
            writer << cp_plstring_ref(y);
        }
      }
    }
    catch (std::ios::failure&)
    {
      return nullptr;
    }

    return writer.finalize(node_name());
  }
};

