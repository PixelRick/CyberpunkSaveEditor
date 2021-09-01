#pragma once
#include <redx/core.hpp>
#include <redx/io/bstream.hpp>

namespace redx {

// memory bstreambuf
struct mem_bstreambuf
  : std::streambuf
{
  ~mem_bstreambuf() override = default;
  mem_bstreambuf() = default;

  explicit mem_bstreambuf(const std::span<const char>& span)
    : m_readonly(true)
  {
    char* const eback = const_cast<char*>(span.data());
    setg(eback, eback, eback + span.size());
    setp(eback, nullptr, eback);
  }

  explicit mem_bstreambuf(const std::span<char>& span, bool readonly = false)
    : m_readonly(readonly)
  {
    char* const eback = span.data();
    setg(eback, eback, eback + span.size());

    if (readonly)
    {
      setp(eback, nullptr, eback);
    }
    else
    {
      setp(eback, eback, eback + span.size());
    }
  }

  FORCE_INLINE std::span<const char> cspan() const
  {
    return span();
  }

  std::span<char> span() const
  {
    std::streamsize size = m_readonly
      ? (egptr() - eback())                  // readonly  -> size == buffer == get area
      : (std::max(m_egp, pptr()) - pbase()); // writeable -> size == written area

    return {eback(), reliable_numeric_cast<size_t>(size)};
  }

  bool reserve(std::streamsize count)
  {
    if (m_buf.get() == eback() && count > capacity())
    {
      resize_buffer(count);
      return true;
    }

    return false;
  }

  std::streamsize capacity() const
  {
    return m_readonly
      ? (egptr() - eback())  // readonly -> buffer == get area
      : (epptr() - pbase()); // writeable -> buffer == put area
  }

protected:

  // only allows growth
  // assumes writeable internal buffer is being used
  void resize_buffer(std::streamsize new_size)
  {
    CP_ASSERT(m_buf.get() == eback());
    CP_ASSERT(!m_readonly);

    const auto old_buf = eback();
    const std::streamsize old_size = epptr() - old_buf;
    if (old_size >= new_size)
    {
      SPDLOG_ERROR("bad logic");
      return;
    }

    const auto new_buf = new char[new_size];
    const ptrdiff_t offset = new_buf - old_buf;

    std::memcpy(new_buf, old_buf, old_size);
    m_buf.reset(new_buf);

    m_egp = std::max(m_egp, pptr()) + offset;
    setp(new_buf, pptr() + offset, new_buf + new_size);
    setg(new_buf, gptr() + offset, m_egp);
  }

#pragma region std::streambuf virtuals

  virtual pos_type seekoff(
    off_type off, std::ios::seekdir dir,
    std::ios::openmode mode = std::ios::in | std::ios::out) override
  {
    const auto eb = eback(); // always equals buffer address
    const auto gp = gptr();
    const auto pp = pptr();

    CP_ASSERT(m_readonly ^ (pp != nullptr));

    // new end of sequence
    if (pp > m_egp)
    {
      m_egp = pp;
    }

    const auto egp_off = m_egp - eb;

    off_type base = 0;
    switch (dir)
    {
      case std::ios::beg:
        break;
      case std::ios::end:
      {
        base = egp_off;
        break;
      }
      case std::ios::cur:
      {
        constexpr auto both = std::ios::in | std::ios::out;
        const auto masked_mode = mode & both;
        if (masked_mode && masked_mode != both)
        {
          if (mode & std::ios::in)
          {
            // gp is always valid
            base = gp - eb;
            break;
          }
          else if (mode & std::ios::out)
          {
            if (pp)
            {
              base = pp - eb;
              break;
            }
          }
        }
        // fall-through to fail
      }
      default:
        return pos_type(-1);
    }

    const off_type new_off = base + off;

    // if (newoff + off) is negative or out of bounds of the initialized part of the buffer, the function fails
    if (new_off < 0 || new_off > egp_off)
    {
      return pos_type(-1);
    }

    // if the pointer to be repositioned is a null pointer and the new offset newoff would be non-zero, this function fails.
    if (new_off != 0 && ((mode & std::ios::out) && !pp))
    {
      return pos_type(-1);
    }

    const auto new_ptr = eb + new_off;

    if (mode & std::ios::in)
    {
      setg(eb, new_ptr, m_egp);
    }

    if ((mode & std::ios::out) && pp)
    {
      setp(eb, new_ptr, epptr());
    }

    return pos_type(new_off);
  }

  virtual pos_type seekpos(
    pos_type pos,
    std::ios::openmode mode = std::ios::in | std::ios::out) override
  {
    return seekoff(reliable_numeric_cast<off_type>(static_cast<size_t>(pos)), std::ios::beg, mode);
  }

  virtual std::streambuf* setbuf(char* data, std::streamsize size) override
  {
    return this;
  }

  // synchronizes the buffers with the associated character sequence
  // virtual int sync() override

  // obtains the number of characters directly available for input in the associated input sequence, if known
  // virtual std::streamsize showmanyc() override

  // reads characters from the associated input sequence to the get area
  virtual int_type underflow() override
  {
    // always readable

    const auto gp = gptr();
    if (gp < egptr())
    {
      return traits_type::to_int_type(*gp);
    }

    const auto new_egp = std::max(m_egp, pptr());
    if (new_egp <= gp)
    {
      // nothing to read from put area either
      return traits_type::eof();
    }

    m_egp = new_egp;
    setg(eback(), gptr(), m_egp);

    return traits_type::to_int_type(*gp);
  }

  // reads characters from the input sequence to the get area and advances the next pointer
  // virtual int_type uflow() override

  // reads multiple characters from the input sequence
  // virtual std::streamsize xsgetn(char* dst, std::streamsize count) override

  // writes character to the output sequence
  virtual int_type overflow(int_type c = traits_type::eof()) override
  {
    if (m_readonly)
    {
      // read-only
      return traits_type::eof();
    }

    if (traits_type::eq_int_type(traits_type::eof(), c))
    {
      return traits_type::not_eof(c); // blank overflow
    }

    const auto pp = pptr();
    const auto ep = epptr();
    if (pp < ep)
    {
      *pp = traits_type::to_char_type(c);
      gbump(1);
      return c;
    }

    if (m_buf.get() != eback())
    {
      // end of external source span
      return traits_type::eof();
    }

    // grow buffer and store element

    std::streamsize old_size = ep - pbase();
    std::streamsize new_size = 0;
    if (old_size < 32)
    {
      new_size = 32;
    }
    else
    {
      new_size = old_size + (old_size >> 1);
      // test overflow
      if (new_size < old_size)
      {
        new_size = std::numeric_limits<std::streamsize>::max();
      }
    }

    CP_ASSERT(new_size > 0);
    if (new_size == old_size)
    {
      // can't grow
      return traits_type::eof();
    }

    resize_buffer(new_size);

    *pptr() = traits_type::to_char_type(c);
    pbump(1);

    return c;
  }

  // writes multiple characters to the output sequence 
  // virtual std::streamsize xsputn(const char* src, std::streamsize count) override

  // puts a character back into the input sequence, possibly modifying the input sequence
  virtual int_type pbackfail(int_type c = traits_type::eof()) override
  {
    const auto gp = gptr();

    if (gp <= eback())
    {
      // can't go back by 1
      return traits_type::eof();
    }

    const bool c_is_eof = traits_type::eq_int_type(traits_type::eof(), c);
    const char cc = traits_type::to_char_type(c);

    if (m_readonly)
    {
      // can only succeed if c == eof or the character at gp[-1]
      if (!c_is_eof && cc != gp[-1])
      {
        return traits_type::eof();
      }
    }

    gbump(-1);

    if (!c_is_eof)
    {
      *gptr() = cc;
    }

    return traits_type::not_eof(c);
  }

#pragma endregion

protected:

  std::unique_ptr<char[]> m_buf;
  char* m_egp = nullptr; // pointer to end
  bool m_readonly = true;
};

struct mem_ibstream
  : ibstream
{
  using bstreambuf_type = mem_bstreambuf;

  explicit mem_ibstream(const char* data, std::streamsize count)
    : ibstream(m_buf), m_buf(std::span<const char>(data, count)) {}

  explicit mem_ibstream(const std::span<const char>& span)
    : ibstream(m_buf), m_buf(span) {}

  explicit mem_ibstream(const std::span<char>& span, bool readonly = false)
    : ibstream(m_buf), m_buf(span, readonly) {}

  FORCE_INLINE std::span<const char> cspan() const
  {
    return m_buf.cspan();
  }

  FORCE_INLINE std::span<char> span() const
  {
    return m_buf.span();
  }

protected:

  bstreambuf_type m_buf;
};

struct mem_obstream
  : obstream
{
  using bstreambuf_type = mem_bstreambuf;

  mem_obstream()
    : obstream(m_buf), m_buf() {}

  explicit mem_obstream(const std::span<char>& span)
    : obstream(m_buf), m_buf(span, false) {}

  FORCE_INLINE std::span<const char> cspan() const
  {
    return m_buf.cspan();
  }

  FORCE_INLINE std::span<char> span() const
  {
    return m_buf.span();
  }

  FORCE_INLINE void reserve(std::streamsize size)
  {
    m_buf.reserve(size);
  }

  FORCE_INLINE std::streamsize capacity() const
  {
    return m_buf.capacity();
  }

protected:

  bstreambuf_type m_buf;
};

} // namespace redx

