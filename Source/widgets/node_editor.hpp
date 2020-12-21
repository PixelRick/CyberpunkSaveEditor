#pragma once
#include "cserialization/node.hpp"

/*
struct node_dataview
{
std::shared_ptr<node_t> node;

virtual void commit() = 0;
virtual void reload() = 0;
};
*/

class node_editor
{
public:
  bool auto_commit = false;

protected:
  std::unique_ptr<node_dataview> m_view;

public:
  void draw(const std::shared_ptr<node_t>& node)
  {
    if (!node)
      return;

    if (node != m_view->node())
    {
      m_view->commit();
      //m_view->node = node;
      m_view->reload();
    }

    draw_impl();
  }

  void commit() { m_view->commit(); };
  void reload() { m_view->reload(); };

protected:
  virtual void draw_impl() = 0;
};

