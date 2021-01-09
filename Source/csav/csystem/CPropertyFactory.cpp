#include "CPropertyFactory.hpp"
#include <string>
#include <functional>
#include <cpinternals/cpenums.hpp>
#include <csav/csystem/CProperty.hpp>


template <typename CPropType, typename ...Args>
std::function<CPropertyUPtr(CPropertyOwner*)> build_prop_creator(Args&& ...args)
{
  return [args...](CPropertyOwner* owner) -> CPropertyUPtr {
    return std::move(std::unique_ptr<CProperty>(new CPropType(owner, args...)));
  };
}

std::function<CPropertyUPtr(CPropertyOwner*)> CPropertyFactory::get_creator(CSysName ctypename)
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

        return build_prop_creator<CArrayProperty>(elt_type, array_size);
      }
      catch (std::exception&)
      {
        // pass
      }
    }
    return build_prop_creator<CUnknownProperty>(ctypename);
  }
  else if (str_ctypename.rfind("array:", 0) == 0)
  {
    CSysName sub_ctypename(str_ctypename.substr(sizeof("array:") - 1));
    return build_prop_creator<CDynArrayProperty>(sub_ctypename);
  }
  else if (str_ctypename.rfind("handle:", 0) == 0)
  {
    CSysName sub_ctypename(str_ctypename.substr(sizeof("handle:") - 1));
    return build_prop_creator<CHandleProperty>(sub_ctypename);
  }
  else if (str_ctypename == "Bool")
  {
    return build_prop_creator<CBoolProperty>();
  }
  else if (str_ctypename == "Uint8")      { return build_prop_creator<CIntProperty>(EIntKind::U8);  }
  else if (str_ctypename == "Int8")       { return build_prop_creator<CIntProperty>(EIntKind::I8);  }
  else if (str_ctypename == "Uint16")     { return build_prop_creator<CIntProperty>(EIntKind::U16); }
  else if (str_ctypename == "Int16")      { return build_prop_creator<CIntProperty>(EIntKind::I16); }
  else if (str_ctypename == "Uint32")     { return build_prop_creator<CIntProperty>(EIntKind::U32); }
  else if (str_ctypename == "Int32")      { return build_prop_creator<CIntProperty>(EIntKind::I32); }
  else if (str_ctypename == "Uint64")     { return build_prop_creator<CIntProperty>(EIntKind::U64); }
  else if (str_ctypename == "Int64")      { return build_prop_creator<CIntProperty>(EIntKind::I64); }
  else if (str_ctypename == "Float")      { return build_prop_creator<CFloatProperty>();            }
  else if (str_ctypename == "TweakDBID")  { return build_prop_creator<CTweakDBIDProperty>();        }
  else if (str_ctypename == "CName")      { return build_prop_creator<CNameProperty>();             }
  else if (str_ctypename == "NodeRef")
  {
    return build_prop_creator<CNodeRefProperty>();
  }
  else if (CEnumList::get().is_registered(str_ctypename))
  {
    return build_prop_creator<CEnumProperty>(ctypename);
  }
  else if (str_ctypename == "gameSavedStatsData")
  {
    return build_prop_creator<CObjectProperty>(ctypename);
  }

  if (str_ctypename.find(':') == std::string::npos)
  {
    return build_prop_creator<CObjectProperty>(ctypename);
  }

  return build_prop_creator<CUnknownProperty>(ctypename);
}

