#pragma once
#include "stringpool.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace redx {

template <uint32_t PoolTag>
struct literal_gstring;

template <uint32_t PoolTag>
struct gstrid;

namespace detail {

// threadsafe global pool
template <uint32_t PoolTag>
struct gstringpool
{
  static stringpool_mt& get()
  {
    static stringpool_mt instance;
    static bool once = [](){

      const uint32_t pooltag = PoolTag;
      auto invalid = fmt::format("<gstring:{}:invalid>", std::string_view((char*)&pooltag, 4));
      instance.register_string(invalid);
      assert(instance.at(0) == invalid);
      s_ppool = &instance;
      return true;

    }();
    return *s_ppool;
  }

private:

  static inline stringpool_mt* s_ppool = nullptr;
};

} // namespace detail

// global string with fast access (by index in global pool)
template <uint32_t PoolTag>
struct gstring
{
  constexpr static uint32_t pool_tag = PoolTag;

  constexpr gstring() noexcept
    : m_idx(0)
  {
  }

  explicit gstring(const char* s)
    : gstring(std::string_view(s)) {}

  explicit gstring(std::string_view s)
  {
    m_idx = nc_gpool().register_string(s).second;
  }

  explicit gstring(const std::string& s)
    : gstring(std::string_view(s))
  {
  }

  gstring(const gstring&) = default;
  gstring& operator=(const gstring&) = default;

  operator bool() const
  {
    return !!m_idx;
  }

  friend inline bool operator<(const gstring& a, const gstring& b)
  {
    return a.strv() < b.strv();
  }

  friend inline bool operator==(const gstring& a, const gstring& b)
  {
    return a.m_idx == b.m_idx;
  }

  friend inline bool operator!=(const gstring& a, const gstring& b)
  {
    return !(a == b);
  }

  // this doesn't verify the hash is correct.
  // use it to feed its global pool from a db without recomputing hashes.
  static gstring register_with_hash(std::string_view s, uint64_t hash)
  {
    return gstring(nc_gpool().register_string(s, hash).second);
  }

  // Returned string_view has static storage duration
  std::string_view strv() const
  {
    return nc_gpool().at(m_idx);
  }

  const char* c_str() const
  {
    // string_pool guarantees a null terminator after the view.
    return strv().data();
  }

  operator std::string() const
  {
    return std::string(strv());
  }

  std::string string() const
  {
    return std::string(strv());
  }

  uint32_t idx() const noexcept { return m_idx; }

  static std::optional<gstring> find(const uint64_t hash)
  {
    auto opt = nc_gpool().find(hash);

    if (opt.has_value())
    {
      return { gstring(opt.value()) };
    }

    return std::nullopt;
  }

  static void pool_reserve(size_t cnt)
  {
    nc_gpool().reserve(cnt);
  }

  //static const stringpool& gpool()
  //{
  //  return detail::gstringpool<pool_tag>::get();
  //}

private:

  friend literal_gstring<PoolTag>;
  friend gstrid<PoolTag>;

  explicit gstring(uint32_t idx) noexcept
    : m_idx(idx) {}

  static stringpool_mt& nc_gpool()
  {
    return detail::gstringpool<pool_tag>::get();
  }

  uint32_t m_idx;
};


template <uint32_t PoolTag>
inline void to_json(nlohmann::json& j, const gstring<PoolTag>& x)
{
  j = x.strv();
}

template <uint32_t PoolTag>
inline void from_json(const nlohmann::json& j, gstring<PoolTag>& x)
{
  x = gstring<PoolTag>(j.get<std::string>());
}


// TODO: rework this with C++20 (single static_gstring per char* value)
template <uint32_t PoolTag>
struct literal_gstring
{
  using gstring_type = gstring<PoolTag>;

  constexpr explicit literal_gstring(const char* const s)
    : str(s)
  {}

  operator const gstring_type&()
  {
    if (!gs)
    {
      gs.m_idx = gstring_type::nc_gpool().register_literal(str);
    }

    return gs;
  }

protected:
  const char* const str;
  gstring_type gs;
};

} // namespace redx

namespace std {

template <uint32_t PoolTag>
struct hash<redx::gstring<PoolTag>>
{
  std::size_t operator()(const redx::gstring<PoolTag>& k) const noexcept
  {
    return hash<uint32_t>()(k.idx());
  }
};

} // namespace std

