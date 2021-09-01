#pragma once
#include <redx/core/platform.hpp>

#include <string>
#include <vector>
#include <unordered_map>

#include <redx/core/gstrid.hpp>
#include <redx/core/gname.hpp>
//#include <redx/serialization/serializer.hpp>

namespace redx {

//--------------------------------------------------------
//  cname
//
// name with average constant access to value (lookup).
struct cname
  : public gstrid<gname::pool_tag>
{
  using base = gstrid<gname::pool_tag>;

  using base::base;
  using base::operator=;

  cname(const gstrid<gname::pool_tag>& other)
    : gstrid<gname::pool_tag>(other) {}

  static cname register_with_hash(std::string_view strv, fnv1a64_t hash)
  {
    return cname(nc_gpool().register_string(strv, hash).first);
  }

  // TODO: move that to serializer.. serialization strategy is there not here
  //friend serializer& operator<<(serializer& ser, cname& cn)
  //{
  //  if (ser.flags() & sermanip::cnamehash)
  //  {
  //    return ser << cn.hash;
  //  }
  //
  //  if (ser.is_reader())
  //  {
  //    std::string s;
  //    ser << s;
  //    cn = cname(s);
  //  }
  //  else
  //  {
  //    auto gs = cn.gstr();
  //    if (!gs)
  //    {
  //      // todo:
  //      // debug break instead ?
  //      // just set error ?
  //      throw std::runtime_error(fmt::format("trying to serialize <cname:{:016X}> as string but the string is unknown", cn.hash));
  //    }
  //    std::string s = gs.string();
  //    ser << s;
  //  }
  //
  //  return ser;
  //}

  std::string string() const
  {
    auto gs = gstr();
    if (!gs)
    {
      return fmt::format("<cname:{:016X}>", hash);
    }
    return gs.string();
  }
};

static_assert(sizeof(cname) == 8);

inline std::ostream& operator<<(std::ostream& os, const cname& x)
{ 
  return os << x.string(); 
}

namespace literals {

using literal_cname_helper = literal_gstring_helper<cname::pool_tag>;

struct cname_builder
  : protected literal_cname_helper::gstrid_builder
{
  using gstrid_type = literal_cname_helper::gstrid_type;

  using literal_cname_helper::gstrid_builder::gstrid_builder;

  inline operator cname() const
  {
    static_assert(std::is_same_v<cname::base, gstrid_type>);
    return cname(this->operator gstrid_type());
  }
};

// does register the name
constexpr cname_builder operator""_cndef(const char* s, std::size_t)
{
  // todo: c++20, use the singleton version of the builder
  return cname_builder(s);
}

// does not register the name
constexpr cname operator""_cn(const char* s, std::size_t n)
{
  // todo: c++20, use the singleton version of the builder
  return cname(fnv1a64(s));
}

inline constexpr auto cn_test1 = literal_cname_helper::builder("test1");
inline constexpr auto cn_test2 = "test2"_cndef;
inline constexpr auto cn_test3 = "test2"_cn;

} // namespace literals

using literals::operator""_cndef;
using literals::operator""_cn;

//--------------------------------------------------------
//  database: sorted names
//  mainly for imgui combo boxes..
//  can be removed when the combo box is reworked properly

struct cname_db
{
  cname_db(const cname_db&) = delete;
  cname_db& operator=(const cname_db&) = delete;

  static cname_db& get()
  {
    static cname_db s = {};
    return s;
  }

  FORCE_INLINE bool is_registered(std::string_view name) const
  {
    return is_registered(cname(name));
  }

  FORCE_INLINE bool is_registered(const cname& cid) const
  {
    return is_registered(cid.hash);
  }

  FORCE_INLINE bool is_registered(uint64_t hash) const
  {
    return gname::find(hash).has_value();
  }

  //bool is_registered_32(uint32_t hash) const
  //{
  //  return m_invmap_32.find(hash) != m_invmap_32.end();
  //}

  void register_str(gname name)
  {
    auto p = insert_sorted_nodupe(m_full_list, name);
    if (!p.second)
      return;
  }

  void register_str(std::string_view name)
  {
    register_str(gname(name));
  }

  //gname resolve_32(uint32_t hash32) const
  //{
  //  auto it = m_invmap_32.find(hash32);
  //  if (it != m_invmap_32.end())
  //    return it->second;
  //  return gname();
  //}

  FORCE_INLINE const std::vector<gname>& sorted_names() const noexcept
  {
    return m_full_list;
  }

  void feed(const std::vector<gname>& names)
  {
    m_full_list.reserve(m_full_list.size() + names.size());
    auto hint = m_full_list.begin(); // names must be pre-sorted
    for (auto& name : names)
    {
      auto p = insert_sorted_nodupe(m_full_list, hint, name);
      hint = p.first;
    }
  }

protected:

  cname_db() noexcept = default;
  ~cname_db() = default;

protected:

  struct identity_op_32 {
    size_t operator()(uint32_t key) const { return key; }
  };

  std::vector<gname> m_full_list;
  //std::unordered_map<uint32_t, gname, identity_op_32> m_invmap_32;
};

} // namespace redx

template <>
struct fmt::formatter<redx::cname>
  : fmt::formatter<string_view>
{
  template <typename FormatContext>
  auto format(const redx::cname& x, FormatContext& ctx)
  {
    return formatter<string_view>::format(x.string(), ctx);
  }
};

namespace std {

template<> struct hash<redx::cname>
{
    std::size_t operator()(const redx::cname& x) const noexcept
    {
        return x.hash;
    }
};

} // namespace std

