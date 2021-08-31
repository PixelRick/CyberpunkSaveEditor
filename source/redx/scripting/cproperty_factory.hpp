#pragma once
#include <string>

#include "fwd.hpp"
#include "iproperty.hpp"
#include "CStringPool.hpp"

class CPropertyFactory
{
private:
  CPropertyFactory()
  {
    // static init here
  }

  ~CPropertyFactory() {}

public:
  CPropertyFactory(const CPropertyFactory&) = delete;
  CPropertyFactory& operator=(const CPropertyFactory&) = delete;

  static CPropertyFactory& get()
  {
    static CPropertyFactory s;
    return s;
  }

public:
  static std::function<CPropertyUPtr(CPropertyOwner*)> get_creator(gname ctypename);
};

