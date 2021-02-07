#pragma once
#include <filesystem>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

//#pragma message ("deprecated header utils.hpp, include cpinternals/common.hpp instead")

#include "cpinternals/common.hpp"

using namespace cp;


#if __has_include(<span>) && (!defined(_HAS_CXX20) or _HAS_CXX20)
#include <span>
#else
#include "span.hpp"
namespace std { using tcb::span; }
#endif




class span_istreambuf
	: public std::streambuf
{
	char* seekhigh = 0;

public:
	span_istreambuf() = default;
	span_istreambuf(const std::span<char>& span)
		: span_istreambuf(span.begin(), span.end()) {}

	span_istreambuf(const char* begin, const char* end)
	{
		auto ncbegin = const_cast<char*>(begin);
		auto ncend = const_cast<char*>(end);
		this->setg(ncbegin, ncbegin, ncend);
		seekhigh = ncend;
	}

	// Positioning 

	std::streampos seekoff(
		std::streamoff off,
		std::ios_base::seekdir way,
		std::ios_base::openmode mode = std::ios_base::in) override
	{
		// MS stringbuf impl without the out part

		const auto gptr_old = this->gptr();
		const auto seeklow  = this->eback();
		const auto seekdist = seekhigh - seeklow;

		switch (way)
		{
			case std::ios_base::beg:
				break;
			case std::ios_base::end:
				off += seekhigh - seeklow;
				break;
			case std::ios_base::cur:
				if (mode & std::ios_base::in && (gptr_old || !seeklow))
				{
					off += gptr_old - seeklow;
					break;
				}
				// fallthrough
			default:
				return std::streampos(-1);
		}

		if (off > seekdist)
			return std::streampos(-1);

		if (off != 0 && (mode & std::ios_base::in) && !gptr_old)
			return std::streampos(-1);

		const auto new_ptr = seeklow + off;
		if ((mode & std::ios_base::in) && gptr_old)
			setg(seeklow, new_ptr, seekhigh);

		return std::streampos(off);
	}

	std::streampos seekpos(
		std::streampos streampos,
		std::ios_base::openmode mode = std::ios_base::in) override
	{
		return seekoff(streampos, std::ios_base::beg, mode);
	}
};


class vector_istreambuf
	: public span_istreambuf
{
public:
	vector_istreambuf() = default;
	vector_istreambuf(const std::vector<char>& vec)
		: vector_istreambuf(vec.begin(), vec.end()) {}

	vector_istreambuf(typename std::vector<char>::const_iterator begin, typename std::vector<char>::const_iterator end)
		: span_istreambuf(&*begin, &*begin + std::distance(begin, end)) {}
};


// does update the parent stream read position
class isubstreambuf
	: public std::streambuf
{
	std::streambuf* m_sbuf;
	std::streampos	m_pos;
	std::streamsize m_len;

public:
	explicit isubstreambuf(std::streambuf* sbuf, std::streampos pos, std::streampos len) :
		m_sbuf(sbuf), m_pos(pos), m_len(len)
	{
		m_sbuf->pubseekpos(pos);
		setbuf(nullptr, 0);
	}

protected:
	// Helpers

	std::streampos parent_gpos() const
	{
		return m_sbuf->pubseekoff(0, std::ios_base::cur, std::ios_base::in);
	}

	// Positioning 

	std::streampos seekoff(
		std::streamoff off,
		std::ios_base::seekdir way,
		std::ios_base::openmode mode = std::ios_base::in) override
	{
		const std::streampos spos = m_sbuf->pubseekoff(0, std::ios_base::cur, mode);

		switch (way)
		{
			case std::ios_base::beg: { off += m_pos;         break; }
			case std::ios_base::cur: { off += spos;          break; }
			case std::ios_base::end: { off += m_pos + m_len; break; }
			default:
				return std::streampos(-1);
		}

		return m_sbuf->pubseekpos(off, mode) - m_pos;
	}

	std::streampos seekpos(
		std::streampos streampos,
		std::ios_base::openmode mode = std::ios_base::in) override
	{
		streampos += m_pos;

		if (streampos > m_pos + m_len)
			return -1;

		return m_sbuf->pubseekpos(streampos, mode) - m_pos;
	}

	// Get area

	std::streamsize showmanyc() override
	{
		const std::streamoff off = parent_gpos() - m_pos;
		if (off < 0 || off >= m_len)
			return 0;
		return std::min(m_sbuf->in_avail(), m_len - off);
	}

	int underflow() override
	{
		const std::streamoff off = parent_gpos() - m_pos;
		if (off >= m_len)
			return traits_type::eof();

		return m_sbuf->sgetc();
	}

	int uflow() override
	{
		const std::streamoff off = parent_gpos() - m_pos;
		if (off >= m_len)
			return traits_type::eof();

		return m_sbuf->sbumpc();
	}

	std::streamsize xsgetn(char* s, std::streamsize count) override
	{
		const std::streamoff off = parent_gpos() - m_pos;
		if (count > m_len - off)
			count = m_len - off;

		return m_sbuf->sgetn(s, count);
	}
};




void replace_all_in_str(std::string& s, const std::string& from, const std::string& to);

std::string u64_to_cpp(uint64_t val);

std::string bytes_to_hex(const void* buf, size_t len);

std::vector<uintptr_t> sse2_strstr_masked(const unsigned char* s, size_t m, const unsigned char* needle, size_t n, const char* mask, size_t maxcnt = 0);
std::vector<uintptr_t> sse2_strstr(const unsigned char* s, size_t m, const unsigned char* needle, size_t n, size_t maxcnt = 0);

std::optional<std::filesystem::path> find_user_saved_games();

