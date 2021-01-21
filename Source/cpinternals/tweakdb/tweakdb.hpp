// Thanks to novomurdo who reversed the tweakdb.bin format

#pragma once
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <numeric>
#include "cpinternals/common.hpp"
#include "cpinternals/ctypes.hpp"
#include "cpinternals/scripting.hpp"
#include "cpinternals/ioarchive/farchive.hpp"
#include "value_pool.hpp"

namespace cp::tdb {

namespace detail {

struct header_t
{
  uint32_t magic                = 0;
  uint32_t five                 = 0;
  uint32_t four                 = 0;
  uint32_t version_hash         = 0; // Hash of all typenames in groups order
  uint32_t variables_offset     = 0;
  uint32_t groups_offset        = 0;
  uint32_t inlgroups_offset     = 0;
  uint32_t packages_offset      = 0;
};

}

struct QuatElem
{
  std::string name;
  std::string type;
  uint64_t uk;

  friend iarchive& operator<<(iarchive& ar, QuatElem& x)
  {
    return ar << x.name << x.type << x.uk;
  }

  friend bool operator<(QuatElem& a, QuatElem& b)
  {
    return a.uk < b.uk;
  }
};

struct Quaternion
{
  uint8_t uk0;
  QuatElem x, y, z, w;
  std::string uk1;

  friend iarchive& operator<<(iarchive& ar, Quaternion& x)
  {
    ar << x.uk0;
    ar << x.x << x.y << x.z << x.w;
    return ar << x.uk1;
  }

  // TODO: that's probably not enough..
  friend bool operator<(Quaternion& a, Quaternion& b)
  {
    return a.x < b.x && a.y < b.y && a.z < b.z && a.w < b.w;
  }
};

struct pool_desc_t
{
  CName ctypename;
  uint32_t len;

  friend iarchive& operator<<(iarchive& ar, pool_desc_t& x)
  {
    ar << armanip::cnamehash;
    ar << x.ctypename << x.len;
    ar << armanip::cnamestr;
    return ar;
  }
};

struct pool_key_t
{
  TweakDBID key;
  uint32_t idx;

  friend iarchive& operator<<(iarchive& ar, pool_key_t& x)
  {
    return ar << x.key << x.idx;
  }
};

template <typename T>
struct pool_t
{
  value_pool<T> vpool;
  std::vector<pool_key_t> keys;

  friend iarchive& operator<<(iarchive& ar, pool_t& x)
  {
    return ar << x.vpool << x.keys;
  }
};

enum class pool_element_kind : uint64_t
{
  string        = "String"_fnv1a64,
  quaternion    = "Quaternion"_fnv1a64,
  array_bool    = "array:Bool"_fnv1a64,
  array_cname   = "array:CName"_fnv1a64,
  array_i32     = "array:Int32"_fnv1a64,
  array_cres    = "array:raRef:CResource"_fnv1a64,
  array_vec2    = "array:Vector2"_fnv1a64,
  array_string  = "array:String"_fnv1a64,
  array_flt     = "array:Float"_fnv1a64,
  array_vec3    = "array:Vector3"_fnv1a64,
  array_tdbid   = "array:TweakDBID"_fnv1a64,
  vec3          = "Vector3"_fnv1a64,
  euler         = "EulerAngles"_fnv1a64,
  vec2          = "Vector2"_fnv1a64,
  tdbid         = "TweakDBID"_fnv1a64,
  cname         = "CName"_fnv1a64,
  cres          = "raRef:CResource"_fnv1a64,
  lockey        = "gamedataLocKeyWrapper"_fnv1a64,
  flt           = "Float"_fnv1a64,
  i32           = "Int32"_fnv1a64,
  boolean       = "Bool"_fnv1a64,
};

template <pool_element_kind EltKind>
struct pool_type {};

// Thinking...

template <> struct pool_type<pool_element_kind::string> { using type = std::string; };
template <> struct pool_type<pool_element_kind::quaternion> { using type = std::string; };
template <> struct pool_type<pool_element_kind::array_bool> { using type = std::string; };
template <> struct pool_type<pool_element_kind::array_cname> { using type = std::string; };
template <> struct pool_type<pool_element_kind::array_i32> { using type = std::string; };
template <> struct pool_type<pool_element_kind::array_cres> { using type = std::string; };
template <> struct pool_type<pool_element_kind::array_vec2> { using type = std::string; };
template <> struct pool_type<pool_element_kind::array_string> { using type = std::string; };
template <> struct pool_type<pool_element_kind::array_flt> { using type = std::string; };
template <> struct pool_type<pool_element_kind::array_vec3> { using type = std::string; };
template <> struct pool_type<pool_element_kind::array_tdbid> { using type = std::string; };
template <> struct pool_type<pool_element_kind::vec3> { using type = std::string; };

struct tweakdb
{
  tweakdb()
  {
    // Add some types to the CNames
    auto& resolver = CName_resolver::get();
    resolver.register_name("String");
    resolver.register_name("Quaternion");
    resolver.register_name("array:Bool");
    resolver.register_name("array:CName");
    resolver.register_name("array:Int32");
    resolver.register_name("array:raRef:CResource");
    resolver.register_name("array:Vector2");
    resolver.register_name("array:String");
    resolver.register_name("array:Float");
    resolver.register_name("array:Vector3");
    resolver.register_name("array:TweakDBID");
    resolver.register_name("Vector3");
    resolver.register_name("EulerAngles");
    resolver.register_name("Vector2");
    resolver.register_name("TweakDBID");
    resolver.register_name("CName");
    resolver.register_name("raRef:CResource");
    resolver.register_name("gamedataLocKeyWrapper");
    resolver.register_name("Float");
    resolver.register_name("Int32");
    resolver.register_name("Bool");
  }

  bool open(std::filesystem::path path)
  {
    ifarchive ifa(path);

    ifa.seek(0, std::ios_base::end);
    uint32_t blob_size = (uint32_t)ifa.tell();
    ifa.seek(0, std::ios_base::beg);

    op_status status = serialize_in(ifa, blob_size);

    return status;
  }

  const std::vector<pool_desc_t>& pools_descs() const
  {
    return m_pools_descs;
  }

public:
  op_status serialize_in(iarchive& ar, size_t blob_size)
  {
    // let's get our header start position
    size_t start_pos = (size_t)ar.tell();

    ar.serialize_pod_raw(m_header);

    auto blob_spos = ar.tell();

    // check header
    if (m_header.five != 5 || m_header.four != 4)
      return false;
    if (m_header.variables_offset > m_header.groups_offset)
      return false;
    if (m_header.groups_offset > m_header.inlgroups_offset)
      return false;
    if (m_header.inlgroups_offset > m_header.packages_offset)
      return false;
    if (m_header.packages_offset > blob_size)
      return false;

    ar << m_pools_descs;

    // TODO check that each pool desc corresponds

    ar << pool_string;
    ar << pool_quats;
    ar << pool_array_bool;
    ar << pool_array_cname;
    ar << pool_array_i32;

    size_t new_pos = (size_t)ar.tell();

    return true;
  }

  pool_t<std::string> pool_string;
  pool_t<Quaternion> pool_quats;
  pool_t<std::vector<bool>> pool_array_bool;
  pool_t<std::vector<CName>> pool_array_cname;
  pool_t<std::vector<int32_t>> pool_array_i32;


  //resolver.register_name("array:raRef:CResource");
  //resolver.register_name("array:Vector2");
  //resolver.register_name("array:String");
  //resolver.register_name("array:Float");
  //resolver.register_name("array:Vector3");
  //resolver.register_name("array:TweakDBID");
  //resolver.register_name("Vector3");
  //resolver.register_name("EulerAngles");
  //resolver.register_name("Vector2");
  //resolver.register_name("TweakDBID");
  //resolver.register_name("CName");
  //resolver.register_name("raRef:CResource");
  //resolver.register_name("gamedataLocKeyWrapper");
  //resolver.register_name("Float");
  //resolver.register_name("Int32");
  //resolver.register_name("Bool");

private:
  detail::header_t m_header;
  std::vector<pool_desc_t> m_pools_descs;
};

} // namespace cp::tdb

