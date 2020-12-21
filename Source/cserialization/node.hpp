#pragma once
#include <string>
#include <vector>
#include <memory>
#include <numeric>

// let's keep it simple for now
// children must keep the original ordering, except for arrays (items)
struct node_t
{
  // empty name means blob
  std::string name;
  std::vector<std::shared_ptr<node_t>> children;
  std::vector<char> data;
  int32_t idx;

  size_t calcsize() const
  {
    size_t base_size = data.size() + (idx >= 0 ? 4 : 0);
    return std::accumulate(
      children.begin(), children.end(), base_size,
      [](size_t cnt, auto& node){ return cnt + node->calcsize(); }
    );
  }

  uint32_t treecount() const
  {
    if (idx >= 0) {
      return std::accumulate(
        children.begin(), children.end(), (uint32_t)1,
        [](uint32_t cnt, auto& node){ return cnt + node->treecount(); }
      );
    }
    return 0;
  }
};

