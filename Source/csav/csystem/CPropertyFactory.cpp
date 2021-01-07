#include "CPropertyFactory.hpp"
#include <string>
#include <functional>
#include <cpinternals/cpenums.hpp>
#include <csav/csystem/CProperty.hpp>

std::function<CPropertySPtr()> CPropertyFactory::get_creator(CSysName ctypename)
{
  std::string str_ctypename = ctypename.str();

  if (str_ctypename.size() && str_ctypename[0] == '[')
  {
    auto pos = str_ctypename.find(']');
    if (pos != std::string::npos)
    {
      try
      {
        size_t array_size = std::stoul(std::string(str_ctypename.substr(1, pos - 1)));
        CSysName elt_type(str_ctypename.substr(pos + 1));
        return [elt_type, array_size]() -> CPropertySPtr {
          return std::make_shared<CArrayProperty>(elt_type, array_size);
        };
      }
      catch (std::exception&)
      {
        // pass
      }
    }

    return [ctypename]() -> CPropertySPtr {
      return std::make_shared<CUnknownProperty>(ctypename);
    };
  }
  else if (str_ctypename.rfind("array:", 0) == 0)
  {
    CSysName sub_ctypename(str_ctypename.substr(sizeof("array:") - 1));
    return [sub_ctypename]() -> CPropertySPtr {
      return std::make_shared<CDynArrayProperty>(sub_ctypename);
    };
  }
  else if (str_ctypename.rfind("handle:", 0) == 0)
  {
    CSysName sub_ctypename(str_ctypename.substr(sizeof("handle:") - 1));
    return [sub_ctypename]() -> CPropertySPtr {
      return std::make_shared<CHandleProperty>(sub_ctypename);
    };
  }
  else if (str_ctypename == "Bool")
  {
    return []() -> CPropertySPtr { return std::make_shared<CBoolProperty>(); };
  }
  else if (str_ctypename == "Uint8")      { return []() -> CPropertySPtr { return std::make_shared<CIntProperty>(EIntKind::U8);  }; }
  else if (str_ctypename == "Int8")       { return []() -> CPropertySPtr { return std::make_shared<CIntProperty>(EIntKind::I8);  }; }
  else if (str_ctypename == "Uint16")     { return []() -> CPropertySPtr { return std::make_shared<CIntProperty>(EIntKind::U16); }; }
  else if (str_ctypename == "Int16")      { return []() -> CPropertySPtr { return std::make_shared<CIntProperty>(EIntKind::I16); }; }
  else if (str_ctypename == "Uint32")     { return []() -> CPropertySPtr { return std::make_shared<CIntProperty>(EIntKind::U32); }; }
  else if (str_ctypename == "Int32")      { return []() -> CPropertySPtr { return std::make_shared<CIntProperty>(EIntKind::I32); }; }
  else if (str_ctypename == "Uint64")     { return []() -> CPropertySPtr { return std::make_shared<CIntProperty>(EIntKind::U64); }; }
  else if (str_ctypename == "Int64")      { return []() -> CPropertySPtr { return std::make_shared<CIntProperty>(EIntKind::I64); }; }
  else if (str_ctypename == "Float")      { return []() -> CPropertySPtr { return std::make_shared<CFloatProperty>();            }; }
  else if (str_ctypename == "TweakDBID")  { return []() -> CPropertySPtr { return std::make_shared<CTweakDBIDProperty>();        }; }
  else if (str_ctypename == "CName")      { return []() -> CPropertySPtr { return std::make_shared<CNameProperty>();             }; }
  else if (str_ctypename == "NodeRef")
  {
    return []() -> CPropertySPtr { return std::make_shared<CNodeRefProperty>(); };
  }
  else if (CEnumList::get().is_registered(str_ctypename))
  {
    return [ctypename]() -> CPropertySPtr {
      return std::make_shared<CEnumProperty>(ctypename);
    };
  }
  else if (str_ctypename == "gameSavedStatsData")
  {
    return [ctypename]() -> CPropertySPtr {
      return std::make_shared<CObjectProperty>(ctypename);
    };
  }

  if (str_ctypename.find(':') == std::string::npos)
  {
    return [ctypename]() -> CPropertySPtr {
      return std::make_shared<CObjectProperty>(ctypename);
    };
  }

  return [ctypename]() -> CPropertySPtr {
    return std::make_shared<CUnknownProperty>(ctypename);
  };
}

CPropertySPtr CPropertyFactory::create(std::string_view ctypename)
{
  if (ctypename.size() && ctypename[0] == '[')
  {
    auto pos = ctypename.find(']');
    if (pos != std::string::npos)
    {
      try
      {
        size_t array_size = std::stoul(std::string(ctypename.substr(1, pos - 1)));
        auto elt_ctypename = CSysName(ctypename.substr(pos + 1));
        return std::make_shared<CArrayProperty>(elt_ctypename, array_size);
      }
      catch (std::exception&)
      {
        // pass
      }
    }
    return std::make_shared<CUnknownProperty>(ctypename);
  }
  else if (ctypename.rfind("array:", 0) == 0)
  {
    auto sub_ctypename = CSysName(ctypename.substr(sizeof("array:") - 1));
    return std::make_shared<CDynArrayProperty>(sub_ctypename);
  }
  else if (ctypename.rfind("handle:", 0) == 0)
  {
    auto sub_ctypename = CSysName(ctypename.substr(sizeof("handle:") - 1));
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
  else if (ctypename == "NodeRef")
  {
    return std::make_shared<CNodeRefProperty>();
  }
  else if (CEnumList::get().is_registered(ctypename))
  {
    return std::make_shared<CEnumProperty>(ctypename);
  }
  else if (ctypename == "gameSavedStatsData")
  {
    return std::make_shared<CObjectProperty>(ctypename);
  }

  if (ctypename.find(':') == std::string::npos)
    return std::make_shared<CObjectProperty>(ctypename);

  return std::make_shared<CUnknownProperty>(ctypename);
}

