#pragma once
#include <optional>
#include <vector>
#include <limits>

#include <redx/core/platform.hpp>
#include <redx/core/utils.hpp>
#include <redx/io/bstream.hpp>

namespace redx {

// descriptor matching cyberpunk impl
//  - StringSizeBits let's you define the count of bits used to store the string length and thus define the maximum length for strings.
//  - the remaining bits are used to define the offset and it constrains the maximum size of the pool.
template <size_t StringSizeBits = 8 /* default max string length is 255 */>
struct relspan_packed
{
protected:

  static constexpr uint32_t offset_mask = (1 << (32 - StringSizeBits)) - 1;
  static constexpr uint32_t size_shift  = 32 - StringSizeBits;

public:

  static constexpr size_t max_offset    = offset_mask;
  static constexpr size_t max_size      = (1 << size_shift) - 1;

  relspan_packed() noexcept = default;

  relspan_packed(size_t offset, size_t size)
  {
    REDX_ASSERT(offset < (std::numeric_limits<uint32_t>::max)() );
    REDX_ASSERT(size < (std::numeric_limits<uint32_t>::max)() );
    this->set_offset(reliable_integral_cast<uint32_t>(offset));
    this->set_size(reliable_integral_cast<uint32_t>(size));
  }

  inline uint32_t offset() const noexcept
  {
    return m_bitfld & offset_mask;
  }

  inline uint32_t size() const noexcept
  {
    return m_bitfld >> size_shift;
  }

  inline uint32_t end_offset() const noexcept
  {
    return offset() + size();
  }

  void set_offset(size_t value)
  {
    if (value > max_offset)
    {
      SPDLOG_CRITICAL("offset is too big");
      DEBUG_BREAK();
    }

    m_bitfld = static_cast<uint32_t>(value) | (m_bitfld & ~offset_mask);
  }

  void set_size(size_t value)
  {
    if (value > max_size)
    {
      SPDLOG_CRITICAL("size is too big");
      DEBUG_BREAK();
    }

    m_bitfld = (static_cast<uint32_t>(value) << size_shift) | (m_bitfld & offset_mask);
  }

protected:

  uint32_t m_bitfld = 0;
};

template <typename OffsetT>
struct relspan
{
  using offset_type = OffsetT;
  using size_type = OffsetT;

  static constexpr size_t max_offset    = std::numeric_limits<offset_type>::max();
  static constexpr size_t max_size      = max_offset;

  relspan() noexcept = default;

  relspan(size_t offset, size_t size)
  {
    REDX_ASSERT(offset < max_offset);
    REDX_ASSERT(size < max_size);
    this->set_offset(offset);
    this->set_size(size);
  }

  inline uint32_t offset() const noexcept
  {
    return m_offset;
  }

  inline uint32_t size() const noexcept
  {
    return m_size;
  }

  inline uint32_t end_offset() const noexcept
  {
    return offset() + size();
  }

  void set_offset(size_t value)
  {
    if (value > max_offset)
    {
      SPDLOG_CRITICAL("offset is too big");
      DEBUG_BREAK();
    }

    m_offset = static_cast<offset_type>(value);
  }

  void set_size(size_t value)
  {
    if (value > max_size)
    {
      SPDLOG_CRITICAL("size is too big");
      DEBUG_BREAK();
    }

    m_size = static_cast<size_type>(value);
  }

protected:

  offset_type m_offset = 0;
  offset_type m_size = 0;
};

// this is a container for strings, it is designed for serialization.
// it does reallocate thus string_views can be invalidated on push_back.
// StringDesc must provide the same interface as relspan
template <class RelSpanT, bool AddNullTerminators = false>
struct stringvec
{
  using str_desc = RelSpanT;
  using block_type = std::vector<char>; // todo: use unique_ptr<char*> to do the initialization ourselves

  stringvec() = default;

  inline void clear()
  {
    m_descs.clear();
    m_block.clear();
  }

  // returns count of strings
  inline uint32_t size() const
  {
    return reliable_integral_cast<uint32_t>(m_descs.size());
  }

  // view can be invalidated on insertion
  inline std::string_view at(size_t idx) const
  {
    const auto& desc = m_descs[idx];
    return std::string_view(m_block.data() + desc.offset(), desc.size());
  }

  // Copies the string at the end of the storage
  // and adds a terminating null character if AddNullTerminators is true.
  void push_back(std::string_view sv)
  {
    const size_t offset = m_block.size();
    if (offset > str_desc::max_offset)
    {
      SPDLOG_CRITICAL("current offset cannot fit in descriptor");
      DEBUG_BREAK();
    }

    // let's use std geometric growth, and provide a shrink_to_fit
    std::copy(sv.begin(), sv.end(), std::back_inserter(m_block));

    if constexpr (AddNullTerminators)
    {
      m_block.push_back('\0');
    }

    m_descs.emplace_back(offset, sv.size());
  }

  inline size_t memory_size() const
  {
    return m_block.size();
  }

  inline const block_type& memory_block() const
  {
    return m_block;
  }

  block_type&& steal_memory_block()
  {
    block_type tmp;
    tmp.swap(m_block);
    clear();
    return std::move(tmp);
  }

  inline void shrink_block_to_fit()
  {
    m_block.shrink_to_fit();
  }

  // maybe enable_if AddNullTerminators==true ?
  template<bool U = AddNullTerminators, typename Enable = std::enable_if_t<U>>
  void assign_block(std::vector<char>&& block)
  {
    m_block = std::move(block);

    const char* const beg = m_block.data();
    const char* const end = beg + m_block.size();
    const char* start = beg;
    for (const char* c = start; c != end; ++c)
    {
      if (*c == '\0')
      {
        m_descs.emplace_back(start - beg, c - start - 1); // null terminator not counted in descriptor
        start = c;
      }
    }
  }

  // matches cyberpunk serialization
  bool serialize_in(ibstream& st, uint32_t base_offset, uint32_t descs_size, uint32_t data_size)
  {
    clear();

    if (descs_size % sizeof(str_desc) != 0)
    {
      STREAM_LOG_AND_SET_ERROR(st, "invalid given descs_size");
      return false;
    }

    const size_t descs_cnt = descs_size / sizeof(str_desc);
    if (descs_cnt == 0)
    {
      if (data_size != 0)
      {
        STREAM_LOG_AND_SET_ERROR(st, "descs_cnt == 0 but data_size != 0");
        return false;
      }
      return true;
    }

    m_descs.resize(descs_cnt);
    st.read_array(m_descs);

    // find end
    uint32_t max_offset = 0;
    for (auto& desc : m_descs)
    {
      const uint32_t end_offset = desc.end_offset();
      if (end_offset > max_offset)
        max_offset = end_offset;
    }

    // iterator is valid
    const uint32_t real_end_offset = base_offset + descs_size + data_size;
    if (max_offset > real_end_offset)
    {
      STREAM_LOG_AND_SET_ERROR(st, "max_offset > real_end_offset");
      return false;
    }

    m_block.resize(data_size);
    st.read_bytes(m_block.data(), data_size);

    if (!st)
    {
      STREAM_LOG_AND_SET_ERROR(st, "couldn't read block");
      m_descs.clear();
      m_block.clear();
      return false;
    }

    // fix desc offsets
    for (auto& desc : m_descs)
    {
      const uint32_t offset = desc.offset();

      if (offset < base_offset + descs_size)
      {
        STREAM_LOG_AND_SET_ERROR(st, "desc offset is lower than expected");
        clear(); // prevent invalid state
        return false;
      }

      desc.set_offset(offset - base_offset - descs_size);

      if (desc.end_offset() > data_size)
      {
        STREAM_LOG_AND_SET_ERROR(st, "desc end offset is bigger than expected");
        clear(); // prevent invalid state
        return false;
      }
    }

    return true;
  }

  bool serialize_out(obstream& st, uint32_t base_offset, uint32_t& out_descs_size, uint32_t& out_data_size)
  {
    // todo: the additional offseting here can make current descriptors offsets overflow..
    //       so it would be nice to detect it early

    out_descs_size = 0;
    out_data_size = 0;

    const uint32_t desc_cnt = (uint32_t)m_descs.size();
    const uint32_t descs_size = (uint32_t)(desc_cnt * sizeof(str_desc));
    const uint32_t data_offset = base_offset + descs_size;
    const uint32_t data_size = (uint32_t)m_block.size();

    std::vector<str_desc> new_descs = m_descs;

    for (auto& desc : new_descs)
    {
      desc.set_offset(desc.offset() + data_offset);
    }

    st.write_array(new_descs);
    st.write_bytes(m_block.data(), data_size);

    out_descs_size = descs_size;
    out_data_size = data_size;

    return bool(st);
  }

private:

  block_type m_block;
  std::vector<str_desc> m_descs;
};

} // namespace redx

