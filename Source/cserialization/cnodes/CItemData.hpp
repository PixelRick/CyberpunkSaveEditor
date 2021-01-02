#pragma once
#include <inttypes.h>
#include <iostream>

#include <cpinternals/cpnames.hpp>
#include <cserialization/node.hpp>
#include <cserialization/serializers.hpp>


#pragma pack(push, 1)

struct uk_thing
{
  uint32_t uk4 = 0;
  uint8_t  uk1 = 0;
  uint16_t uk2 = 0;

  friend std::istream& operator>>(std::istream& reader, uk_thing& kt)
  {
    reader >> cbytes_ref(kt.uk4);
    reader >> cbytes_ref(kt.uk1);
    reader >> cbytes_ref(kt.uk2);
    return reader;
  }

  friend std::ostream& operator<<(std::ostream& writer, uk_thing& kt)
  {
    writer << cbytes_ref(kt.uk4);
    writer << cbytes_ref(kt.uk1);
    writer << cbytes_ref(kt.uk2);
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

struct CItemID
{
  TweakDBID nameid;
  uk_thing uk;

  CItemID()
    : nameid(), uk() {}

  friend std::istream& operator>>(std::istream& reader, CItemID& iid)
  {
    reader >> cbytes_ref(iid.nameid.as_u64);
    reader >> iid.uk;
    return reader;
  }

  friend std::ostream& operator<<(std::ostream& writer, CItemID& iid)
  {
    writer << cbytes_ref(iid.nameid.as_u64);
    writer << iid.uk;
    return writer;
  }

  std::string name() const
  {
    return nameid.name();
  }

  std::string shortname() const
  {
    return nameid.name();
  }
};

#pragma pack(pop)

struct CItemMod // for CItemData kind 0, 2
{
  CItemID iid;
  char uk0[256];
  TweakDBID uk1;
  std::list<CItemMod> subs;
  uint32_t uk2 = 0;

  TweakDBID uk3;     //
  uint32_t uk4 = 0; // is read as a whole
  uint32_t uk5 = 0x7F7FFFFF; //

  CItemMod()
    : iid(), uk1(), uk3()
  {
    std::fill(uk0, uk0 + sizeof(uk0), 0);
  }

  friend std::istream& operator>>(std::istream& reader, CItemMod& d2)
  {
    reader >> d2.iid;

    std::string s;
    reader >> cp_plstring_ref(s);
    strcpy(d2.uk0, s.c_str());
    reader >> d2.uk1;

    size_t cnt = 0;
    reader >> cp_packedint_ref((int64_t&)cnt);
    d2.subs.resize(cnt);
    for (auto& sub : d2.subs)
      reader >> sub;

    reader >> cbytes_ref(d2.uk2);
    reader >> d2.uk3;
    reader >> cbytes_ref(d2.uk4);
    reader >> cbytes_ref(d2.uk5);
    return reader;
  }

  friend std::ostream& operator<<(std::ostream& writer, CItemMod& d2)
  {
    writer << d2.iid;

    std::string s = d2.uk0;
    writer << cp_plstring_ref(s);
    writer << d2.uk1;

    const size_t cnt = d2.subs.size();
    writer << cp_packedint_ref((int64_t&)cnt);
    for (auto& sub : d2.subs)
      writer << sub;

    writer << cbytes_ref(d2.uk2);
    writer << d2.uk3;
    writer << cbytes_ref(d2.uk4);
    writer << cbytes_ref(d2.uk5);

    return writer;
  }
};

struct CItemData
{
  // CP 1.06
  // serial func at vtbl+0x128
  // ikind0 vtbl 0x143244E30 -> 0x143244F58
  // ikind1 vtbl 0x143244A20 -> 0x143244B48
  // ikind2 vtbl 0x143244C28 -> 0x143244D50

  CItemID iid;

  uint8_t  uk0_012 = 0;
  uint32_t uk1_012 = 0;

  // kind 0,1 stuff
  uint32_t uk2_01 = 0;

  // kind 0,2 stuff
  TweakDBID uk3_02;    //
  uint32_t uk4_02 = 0; // is read as a whole
  uint32_t uk5_02 = 0; //

  CItemMod root2;

  csav_version ver;

  bool from_node(const std::shared_ptr<const node_t>& node, const csav_version& version)
  {
    if (!node)
      return false;

    node_reader reader(node, version);
    reader >> iid;
    auto kind = iid.uk.kind();

    reader >> cbytes_ref(uk0_012);
    reader >> cbytes_ref(uk1_012);

    if (kind != 2)
      reader >> cbytes_ref(uk2_01);

    if (kind != 1)
    {
      reader >> uk3_02;
      reader >> cbytes_ref(uk4_02);
      reader >> cbytes_ref(uk5_02);
      reader >> root2;
    }

    return true;
  }

  std::shared_ptr<const node_t> to_node(const csav_version& version)
  {
    node_writer writer(version);
    writer << iid;
    auto kind = iid.uk.kind();

    writer << cbytes_ref(uk0_012);
    writer << cbytes_ref(uk1_012);

    if (kind != 2)
      writer << cbytes_ref(uk2_01);

    if (kind != 1)
    {
      writer << uk3_02;
      writer << cbytes_ref(uk4_02);
      writer << cbytes_ref(uk5_02);
      writer << root2;
    }

    return writer.finalize("itemData");
  }

  std::string name() const
  {
    return iid.name();
  }
};


