#pragma once
#include <inttypes.h>
#include <string>
#include <algorithm>

namespace cp {

struct task_progress
{
protected:
	struct sub_progress
	{
		std::string comment;
		float min;
		float max;
	};

public:
	task_progress() noexcept
		: main({ "processing...", 0.f, 1.f })
	{
	}

	void push_max(float value)
	{
		const float clamped = std::clamp(value, m_value, 1.f);
		auto& subp = current_sub_progress();
		stack.emplace_back(sub_progress{ subp.comment, m_value, clamped });
	}

	void pop_max(float value)
	{
		if (stack.size())
		{
			m_value = stack.back().max;
			stack.pop_back();
		}
		// should throw
	}

	void set_comment(std::string_view comment)
	{
		auto& subp = current_sub_progress();
		subp.comment = comment;
	}

	std::string comment()
	{
		auto& subp = current_sub_progress();
		return subp.comment;
	}

	void advance(float value)
	{
		const float clamped = std::clamp(value, 0.f, 1.f);
		auto& subp = current_sub_progress();
		m_value = subp.min + (subp.max - subp.min) * clamped;
	}

	float normalized_value() const
	{
		return m_value;
	}

protected:
	sub_progress& current_sub_progress()
	{
		if (stack.size())
		{
			return stack.back();
		}
		return main;
	}

	float m_value = 0.f;
	std::vector<sub_progress> stack;
	sub_progress main;
};

// todo: deprecate
struct progress_t
{
	float value = 0.f;
	std::string comment;
};

// To replace exceptions, return this instead of a bool !
struct op_status
{
	op_status() noexcept = default;

	op_status(std::string_view err)
		: m_err(err)
	{
	}

	op_status(bool ok)
		: m_err(ok ? "" : "uncommented error")
	{
	}

	operator bool() const
	{
		return m_err.empty();
	}

	std::string err() const
	{
		return m_err;
	}

protected:
	std::string m_err;
};

} // namespace cp

