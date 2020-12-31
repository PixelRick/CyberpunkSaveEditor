#pragma once
#include <iostream>
#include <stdint.h>

#include "cserialization/cpnames.hpp"
#include "cserialization/node.hpp"
#include "cserialization/packing.hpp"

#pragma pack(push, 1)

struct uk_thing
{
  uint32_t uk4 = 0;
  uint8_t  uk1 = 0;
  uint16_t uk2 = 0;

  template <typename IStream>
  friend IStream& operator>>(IStream& reader, uk_thing& kt)
  {
    reader.read((char*)&kt.uk4, 4);
    reader.read((char*)&kt.uk1, 1);
    reader.read((char*)&kt.uk2, 2);
    return reader;
  }

  template <typename OStream>
  friend OStream& operator<<(OStream& writer, uk_thing& kt)
  {
    writer.write((char*)&kt.uk4, 4);
    writer.write((char*)&kt.uk1, 1);
    writer.write((char*)&kt.uk2, 2);
    return writer;
  }

  uint8_t kind() const
  {
    if (uk1 == 1) return 2;
    if (uk1 == 2) return 1;
    if (uk1 == 3) return 0;
    return uk4 != 2 ? 2 : 1;
  }
};

struct item_id
{
  namehash nameid;
  uk_thing uk;

  item_id()
    : nameid(), uk() {}

  template <typename IStream>
  friend IStream& operator>>(IStream& reader, item_id& iid)
  {
    reader.read((char*)&iid.nameid.as_u64, 8);
    reader >> iid.uk;
    return reader;
  }

  template <typename OStream>
  friend OStream& operator<<(OStream& writer, item_id& iid)
  {
    writer.write((char*)&iid.nameid.as_u64, 8);
    writer << iid.uk;
    return writer;
  }

  std::string name() const
  {
    return nameid.name();
  }

  std::string shortname() const
  {
    return nameid.shortname();
  }
};

#pragma pack(pop)

struct item_mod // for itemData kind 0, 2
{
  item_id iid;
  char uk0[256];
  namehash uk1;
  std::vector<item_mod> subs;
  uint32_t uk2 = 0;
  namehash uk3;
  uint32_t uk4 = 0;
  uint32_t uk5 = 0;

  item_mod()
    : iid(), uk1(), uk3()
  {
    std::fill(uk0, uk0 + sizeof(uk0), 0);
  }

  template <typename IStream>
  friend IStream& operator>>(IStream& reader, item_mod& d2)
  {
    reader >> d2.iid;
    auto s = read_str(reader);
    strcpy(d2.uk0, s.c_str());
    reader >> d2.uk1;
    const size_t cnt = read_packed_int(reader);
    d2.subs.resize(cnt);
    for (auto& sub : d2.subs)
      reader >> sub;
    reader.read((char*)&d2.uk2, 4);
    reader >> d2.uk3;
    reader.read((char*)&d2.uk4, 4);
    reader.read((char*)&d2.uk5, 4);
    return reader;
  }

  template <typename OStream>
  friend OStream& operator<<(OStream& writer, item_mod& d2)
  {
    writer << d2.iid;
    write_str(writer, d2.uk0);
    writer << d2.uk1;
    write_packed_int(writer, d2.subs.size());
    for (auto& sub : d2.subs)
      writer << sub;
    writer.write((char*)&d2.uk2, 4);
    writer << d2.uk3;
    writer.write((char*)&d2.uk4, 4);
    writer.write((char*)&d2.uk5, 4);
    return writer;
  }
};

struct itemData
{
  // CP 1.06
  // serial func at vtbl+0x128
  // ikind0 vtbl 0x143244E30 -> 0x143244F58
  // ikind1 vtbl 0x143244A20 -> 0x143244B48
  // ikind2 vtbl 0x143244C28 -> 0x143244D50

  item_id iid;

  uint8_t  uk0_012 = 0;
  uint32_t uk1_012 = 0;

  // kind 0,1 stuff
  uint32_t uk2_01 = 0;

  // kind 0,2 stuff
  namehash uk3_02;
  uint32_t uk4_02 = 0;
  uint32_t uk5_02 = 0;
  item_mod root2;

  bool from_node(const std::shared_ptr<const node_t>& node)
  {
    if (!node)
      return false;

    node_reader reader(node);
    reader >> iid;
    auto kind = iid.uk.kind();

    reader.read((char*)&uk0_012, 1);
    reader.read((char*)&uk1_012, 4);

    if (kind != 2)
      reader.read((char*)&uk2_01, 4);

    if (kind != 1)
    {
      reader >> uk3_02;
      reader.read((char*)&uk4_02, 4);
      reader.read((char*)&uk5_02, 4);
      reader >> root2;
    }

    return true;
  }

  std::shared_ptr<const node_t> to_node()
  {
    node_writer writer;
    writer << iid;
    auto kind = iid.uk.kind();

    writer.write((char*)&uk0_012, 1);
    writer.write((char*)&uk1_012, 4);

    if (kind != 2)
      writer.write((char*)&uk2_01, 4);

    if (kind != 1)
    {
      writer << uk3_02;
      writer.write((char*)&uk4_02, 4);
      writer.write((char*)&uk5_02, 4);
      writer << root2;
    }

    return writer.finalize("itemData");
  }

  std::string name()
  {
    iid.name();
  }
};


