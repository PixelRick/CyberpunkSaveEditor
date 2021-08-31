#pragma once
#include <inttypes.h>
#include <string>
#include <memory>
#include <algorithm>
#include <optional>
#include <spdlog/spdlog.h>

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

  float pop_max()
  {
    if (stack.size())
    {
      m_value = stack.back().max;
      stack.pop_back();
      return m_value;
    }
    // should throw
  }

  void set_comment(std::string comment)
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

protected:

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

  op_status(const std::string_view& err)
  {
    if (err != "")
    {
      m_err = err;
    }
  }

  op_status(std::string err)
  {
    if (err != "")
    {
      m_err = err;
    }
  }

  op_status(bool ok)
  {
    if (!ok)
    {
      m_err = "uncommented error";
    }
  }

  op_status& operator=(const op_status& other) = default;

  // returns true when status is "ok"
  operator bool() const
  {
    return !m_err.has_value();
  }

  std::string err() const
  {
    return m_err.value_or("");
  }

protected:
  std::optional<std::string> m_err;
};

} // namespace cp

