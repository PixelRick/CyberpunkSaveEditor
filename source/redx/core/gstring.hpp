#pragma once
#include <redx/core/platform.hpp>

#include <nlohmann/json.hpp>

#include <redx/core/stringpool.hpp>

namespace redx {


template <uint32_t PoolTag>
struct literal_gstring_helper;

template <uint32_t PoolTag>
struct gstrid;

namespace detail {

// threadsafe global pool
template <uint32_t PoolTag>
struct gstringpool
{
  using stringpool_type = stringpool_mt<stringpool_variant::default_u32_indices>;

  static stringpool_type& get()
  {
    static stringpool_type instance = {};
    /*static bool once = [](){

      const uint32_t pooltag = PoolTag;
      auto invalid = fmt::format("<gstring:{}:invalid>", std::string_view((char*)&pooltag, 4));
      instance.register_string(invalid);
      assert(instance.at(0) == invalid);
      return true;

    }();*/
    return instance;
  }
};

} // namespace detail

// global string with fast access (by index in global pool)
template <uint32_t PoolTag>
struct gstring
{
  static constexpr uint32_t pool_tag = PoolTag;
  using stringpool_type = typename detail::gstringpool<pool_tag>::stringpool_type;
  using index_type = typename stringpool_type::index_type;

  constexpr gstring() noexcept = default;

  explicit gstring(const char* s)
    : gstring(std::string_view(s)) {}

  explicit gstring(std::string_view s)
  {
    m_idx = nc_gpool().register_string(s).second;
  }

  explicit gstring(const std::string& s)
    : gstring(std::string_view(s)) {}

  gstring(const gstring&) = default;
  gstring& operator=(const gstring&) = default;

  explicit FORCE_INLINE operator bool() const noexcept
  {
    return !m_idx.is_null();
  }

  friend FORCE_INLINE bool operator<(const gstring& a, const gstring& b)
  {
    return a.strv() < b.strv();
  }

  friend FORCE_INLINE bool operator==(const gstring& a, const gstring& b) noexcept
  {
    return a.m_idx == b.m_idx;
  }

  friend FORCE_INLINE bool operator!=(const gstring& a, const gstring& b) noexcept
  {
    return !(a == b);
  }

  // this doesn't verify the hash is correct.
  // use it to feed its global pool from a db without recomputing hashes.
  static gstring register_with_hash(std::string_view strv, fnv1a64_t hash)
  {
    return gstring(nc_gpool().register_string(strv, hash).second);
  }

  // Returned string_view has static storage duration
  std::string_view strv() const
  {
    return nc_gpool().at(m_idx);
  }

  FORCE_INLINE const char* c_str() const
  {
    // string_pool guarantees a null terminator after the view.
    return strv().data();
  }

  inline operator std::string() const
  {
    return std::string(strv());
  }

  inline std::string string() const
  {
    return std::string(strv());
  }

  FORCE_INLINE fnv1a64_t hash() const noexcept
  {
    return m_idx.hash();
  }

  static std::optional<gstring> find(const fnv1a64_t hash)
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

protected:

  friend literal_gstring_helper<PoolTag>;
  friend gstrid<PoolTag>;

  explicit gstring(index_type idx) noexcept
    : m_idx(idx) {}

  static stringpool_type& nc_gpool()
  {
    return detail::gstringpool<pool_tag>::get();
  }

private:

  index_type m_idx = {};
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

template <uint32_t PoolTag>
inline std::ostream& operator<<(std::ostream& os, const gstring<PoolTag>& x)
{ 
  return os << x.string(); 
}

// todo: check how it gets compiled
//       maybe try a character pack version..

template <uint32_t PoolTag>
struct literal_gstring_helper
{
  using gstring_type = gstring<PoolTag>;
  using gstrid_type = gstrid<PoolTag>;

  struct builder
  {
    constexpr builder(const char* s) noexcept
      : str(nullptr), size(0), hash()
    {
      std::string_view sv(s);
      str = s;
      size = sv.size();
      hash = fnv1a64(sv);
    }
  
    const char* str;
    size_t size;
    fnv1a64_t hash;
  };

#if __cpp_nontype_template_args >= 201911 && !defined(__INTELLISENSE__)

  template <builder Builder>
  struct singleton
  {
    struct gstring_builder
    {
      inline operator gstring_type() const
      {
        return instance().get_gstring();
      }
    };

    struct gstrid_builder
    {
      inline operator gstrid_type() const
      {
        return instance().get_gstrid();
      }
    };

    static const singleton& instance()
    {
      static singleton inst;
      return inst;
    }

    inline gstring_type get_gstring() const
    {
      return gs;
    }

    inline gstrid_type get_gstrid() const
    {
      return gstrid_type(Builder.hash);
    }

  protected:

    singleton()
    {
      gs = gstring_type::register_with_hash(std::string_view(Builder.str, Builder.size), Builder.hash);
    }

    gstring_type gs;
  };

#else

  struct gstring_builder
    : builder
  {
    using builder::builder;
    /*constexpr gstring_builder(const char* s) noexcept
      : builder(s) {}*/

    inline operator gstring_type() const
    {
      return gstring_type::register_with_hash(
        std::string_view(builder::str, builder::size), builder::hash);
    }
  };

  struct gstrid_builder
    : builder
  {
    using builder::builder;
    /*constexpr gstrid_builder(const char* s) noexcept
      : builder(s) {}*/

    inline operator gstrid_type() const
    {
      gstring_type::register_with_hash(std::string_view(builder::str, builder::size), builder::hash);
      return gstrid_type(builder::hash);
    }
  };

#endif
};

} // namespace redx

template <uint32_t PoolTag>
struct std::hash<redx::gstring<PoolTag>>
{
  std::size_t operator()(const redx::gstring<PoolTag>& k) const noexcept
  {
    return k.hash();
  }
};

template <uint32_t PoolTag>
struct fmt::formatter<redx::gstring<PoolTag>>
  : fmt::formatter<string_view>
{
  template <typename FormatContext>
  auto format(const redx::gstring<PoolTag>& x, FormatContext& ctx)
  {
    return formatter<string_view>::format(x.strv(), ctx);
  }
};
