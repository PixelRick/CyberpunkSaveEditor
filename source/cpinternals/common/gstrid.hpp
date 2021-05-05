#pragma once
#include <inttypes.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <cpinternals/common/gstring.hpp>
#include <cpinternals/common/streambase.hpp>

namespace cp {

template <uint32_t PoolTag>
struct gstrid_db;

//--------------------------------------------------------f
//  gstrid
// 
// identifier with average constant access to string (lookup).
template <uint32_t PoolTag>
struct gstrid
{
  constexpr static uint32_t pool_tag = PoolTag;
  using gstring_type = typename gstring<pool_tag>;
  using db_type = typename gstrid_db<pool_tag>;

  gstrid() = default;
  gstrid(const gstrid&) = default;

  constexpr explicit gstrid(uint64_t hash)
    : hash(hash)
  {
  }

  explicit gstrid(const char* sz)
    : gstrid(std::string_view(sz))
  {
  }

  explicit gstrid(std::string_view strv)
  {
    hash = nc_gpool().register_string(strv).first;
  }

  explicit gstrid(gstring_type gs)
  {
    std::string_view strv = gs.strv();
    hash = fnv1a64(strv);
  }

  operator bool() const
  {
    return !!hash;
  }

  gstrid& operator=(const gstrid&) = default;

  gstrid& operator=(std::string_view name)
  {
    operator=(gstrid(name));
    return *this;
  }

  gstrid& operator=(const char* name)
  {
    operator=(gstrid(name));
    return *this;
  }

  gstrid& operator=(gstring_type name)
  {
    operator=(gstrid(name));
    return *this;
  }

  friend bool operator==(const gstrid& a, const gstrid& b)
  {
    return a.hash == b.hash;
  }

  friend bool operator!=(const gstrid& a, const gstrid& b)
  {
    return !(a == b);
  }

  friend bool operator<(const gstrid& a, const gstrid& b)
  {
    return a.hash < b.hash;
  }

  std::string string() const
  {
    auto gs = gstr();
    if (!gs)
    {
      return fmt::format("<{:016X}>", hash);
    }
    return gs.string();
  }

  gstring_type gstr() const
  {
    return gstr_opt().value_or(gstring_type());
  }

  std::optional<gstring_type> gstr_opt() const
  {
    return gstring_type::find(hash);
  }

  uint64_t hash = 0;

protected:

  static stringpool& nc_gpool()
  {
    return gstring_type::nc_gpool();
  }
};

static_assert(sizeof(gstrid<0>) == 8);

} // namespace cp

namespace std {

template <uint32_t PoolTag>
struct hash<cp::gstrid<PoolTag>>
{
  std::size_t operator()(const cp::gstrid<PoolTag>& k) const noexcept
  {
    return k.hash;
  }
};

} // namespace std

