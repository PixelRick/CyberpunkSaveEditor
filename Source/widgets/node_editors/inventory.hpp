#pragma once
#include "node_editor.hpp"

class inventory_editor
  : public node_editor
{
  // attrs
  uint32_t cnt;

public:
  inventory_editor(const std::shared_ptr<const node_t>& node)
    : node_editor(node)
  {
    reload();
  }

public:
  void commit() override
  {
  }

  void reload() override
  {
    auto& children = node()->children();
    const auto& buf = node_data_buffer();

    vector_streambuf<char> c0buf(children[0]->nonconst().data());
    std::istream is(&c0buf);

    is.read((char*)&cnt, 4);

    uint64_t c; is.read((char*)&c, 8);
    uint32_t d; is.read((char*)&d, 4);
  }

protected:
  void draw_impl(const ImVec2& size) override
  {
    ImGui::Text("todo, %d", cnt);
  }

  void on_dirty(size_t offset, size_t len, size_t patch_len) override
  {
    
  }
};

