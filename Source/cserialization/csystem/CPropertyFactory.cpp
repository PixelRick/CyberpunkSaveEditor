#include "CPropertyFactory.hpp"
#include <string>
#include <cpinternals/cpenums.hpp>
#include <cserialization/csystem/CProperty.hpp>


CPropertySPtr CPropertyFactory::create(std::string_view ctypename)
{
  if (ctypename.rfind("array:", 0) == 0)
  {
    auto sub_ctypename = ctypename.substr(sizeof("array:") - 1);
    return std::make_shared<CDynArrayProperty>(sub_ctypename);
  }
  else if (ctypename.rfind("handle:", 0) == 0)
  {
    auto sub_ctypename = ctypename.substr(sizeof("handle:") - 1);
    return std::make_shared<CHandleProperty>(sub_ctypename);
  }
  else if (ctypename == "Bool")
  {
    return std::make_shared<CBoolProperty>();
  }
  else if (ctypename == "Uint8")      { return std::make_shared<CIntProperty>(EIntKind::U8);  }
  else if (ctypename == "Int8")       { return std::make_shared<CIntProperty>(EIntKind::I8);  }
  else if (ctypename == "Uint16")     { return std::make_shared<CIntProperty>(EIntKind::U16); }
  else if (ctypename == "Int16")      { return std::make_shared<CIntProperty>(EIntKind::I16); }
  else if (ctypename == "Uint32")     { return std::make_shared<CIntProperty>(EIntKind::U32); }
  else if (ctypename == "Int32")      { return std::make_shared<CIntProperty>(EIntKind::I32); }
  else if (ctypename == "Uint64")     { return std::make_shared<CIntProperty>(EIntKind::U64); }
  else if (ctypename == "Int64")      { return std::make_shared<CIntProperty>(EIntKind::I64); }
  else if (ctypename == "Float")      { return std::make_shared<CFloatProperty>(); }
  else if (ctypename == "TweakDBID")  { return std::make_shared<CTweakDBIDProperty>(); }
  else if (ctypename == "CName")      { return std::make_shared<CNameProperty>(); }
  else if (ctypename == "NodeRef")    { return std::make_shared<CNodeRefProperty>(); }
  else if (CEnumList::get().is_registered(ctypename))
  {
    return std::make_shared<CEnumProperty>(ctypename);
  }
  else if (ctypename == "gameSavedStatsData")
  {
    return std::make_shared<CObjectProperty>(ctypename);
  }

  return std::make_shared<CObjectProperty>(ctypename);
}

