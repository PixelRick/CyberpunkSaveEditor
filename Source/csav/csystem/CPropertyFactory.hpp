#pragma once
#include <string>
#include <csav/csystem/fwd.hpp>
#include <csav/csystem/CPropertyBase.hpp>
#include <csav/csystem/CStringPool.hpp>

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
  static std::function<CPropertyUPtr(CPropertyOwner*)> get_creator(CSysName ctypename);
};

