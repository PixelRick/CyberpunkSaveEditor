#pragma once
#include <redx/core/platform.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <stdexcept>

#include <redx/core/gstring.hpp>

namespace redx {

template <uint32_t PoolTag>
struct gstrid_db;

//--------------------------------------------------------
//  gstrid
// 
// identifier with average constant access to string (lookup).
template <uint32_t PoolTag>
struct gstrid
{
  static constexpr uint32_t pool_tag = PoolTag;
  using gstring_type = gstring<pool_tag>;
  using db_type = gstrid_db<pool_tag>;

  constexpr gstrid() noexcept = default;
  constexpr gstrid(const gstrid&) noexcept = default;
  constexpr gstrid(gstrid&&) noexcept = default;

  constexpr explicit gstrid(fnv1a64_t hash)
    : hash(hash)
  {
  }

  explicit gstrid(const char* sz, bool register_in_db = false)
    : gstrid(std::string_view(sz), register_in_db)
  {
  }

  explicit gstrid(std::string_view strv, bool register_in_db = false)
  {
    if (register_in_db)
    {
      hash = nc_gpool().register_string(strv).first;
    }
    else
    {
      hash = fnv1a64(strv);
    }
  }

  explicit gstrid(const gstring_type& gs)
  {
    std::string_view strv = gs.strv();
    hash = fnv1a64(strv);
  }

  explicit FORCE_INLINE operator bool() const noexcept
  {
    return !!hash;
  }

  FORCE_INLINE bool operator!() const noexcept
  {
    return !hash;
  }

  constexpr gstrid& operator=(const gstrid&) noexcept = default;
  constexpr gstrid& operator=(gstrid&&) noexcept = default;

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

  gstrid& operator=(const gstring_type& name)
  {
    operator=(gstrid(name));
    return *this;
  }

  friend FORCE_INLINE bool operator==(const gstrid& a, const gstrid& b) noexcept
  {
    return a.hash == b.hash;
  }

  friend FORCE_INLINE bool operator!=(const gstrid& a, const gstrid& b) noexcept
  {
    return !(a == b);
  }

  friend FORCE_INLINE bool operator<(const gstrid& a, const gstrid& b) noexcept
  {
    return a.hash < b.hash;
  }

  static gstrid register_with_hash(std::string_view strv, fnv1a64_t hash)
  {
    return gstrid(nc_gpool().register_string(strv, hash).first);
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

  fnv1a64_t hash = 0;

protected:

  static typename gstring_type::stringpool_type& nc_gpool()
  {
    return gstring_type::nc_gpool();
  }
};

static_assert(sizeof(gstrid<0>) == 8);

template <uint32_t PoolTag>
inline std::ostream& operator<<(std::ostream& os, const gstrid<PoolTag>& x)
{ 
  return os << x.string(); 
}

} // namespace redx

template <uint32_t PoolTag>
struct std::hash<redx::gstrid<PoolTag>>
{
  std::size_t operator()(const redx::gstrid<PoolTag>& k) const noexcept
  {
    return k.hash;
  }
};

template <uint32_t PoolTag>
struct fmt::formatter<redx::gstrid<PoolTag>>
  : fmt::formatter<string_view>
{
  template <typename FormatContext>
  auto format(const redx::gstrid<PoolTag>& x, FormatContext& ctx)
  {
    return formatter<string_view>::format(x.string(), ctx);
  }
};

