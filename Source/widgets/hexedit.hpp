#pragma once
#include "node_editor.hpp"

class hexeditor
  : public node_editor
{
  void commit() override
  {
    
  }

  void reload() override 
  {
    auto& buf = buffer();

  }

protected:
  void draw_impl() override
  {
    
  }

  void on_dirty(size_t offset, size_t len, size_t patch_len) override
  {
    auto& buf = buffer();
  }
};

