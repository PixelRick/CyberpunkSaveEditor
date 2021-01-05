#pragma once
#include <string>
#include <cserialization/csystem/CPropertyBase.hpp>

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
  // ideally the name should be stored in the owner
  static CPropertySPtr create(std::string_view ctypename);
};

