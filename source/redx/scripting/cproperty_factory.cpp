#include <redx/scripting/cproperty_factory.h>

#include <functional>
#include <string>

#include <redx/ctypes.hpp>

#include <redx/scripting/cproperty.h>

namespace redx {

template<typename CPropType, typename... Args>
std::function<CPropertyUPtr(CPropertyOwner*)> build_prop_creator(Args&&... args) {
    return [args...](CPropertyOwner* owner) -> CPropertyUPtr {
        return std::move(std::unique_ptr<CProperty>(new CPropType(owner, args...)));
    };
}

std::function<CPropertyUPtr(CPropertyOwner*)> CPropertyFactory::get_creator(gname ctypename) {
    std::string str_ctypename = ctypename.c_str();

    if (str_ctypename.size() && str_ctypename[0] == '[') {
        auto pos = str_ctypename.find(']');
        if (pos != std::string::npos) {
            try {
                size_t array_size = std::stoul(std::string(str_ctypename.substr(1, pos - 1)));
                gname  elt_type(str_ctypename.substr(pos + 1));

                return build_prop_creator<CArrayProperty>(elt_type, array_size);
            }
            catch (std::exception&) {
                // pass
            }
        }
        return build_prop_creator<CUnknownProperty>(ctypename);
    }
    else if (str_ctypename.rfind("array:", 0) == 0) {
        gname sub_ctypename(str_ctypename.substr(sizeof("array:") - 1));
        return build_prop_creator<CDynArrayProperty>(sub_ctypename);
    }
    else if (str_ctypename.rfind("handle:", 0) == 0) {
        gname sub_ctypename(str_ctypename.substr(sizeof("handle:") - 1));
        return build_prop_creator<CHandleProperty>(sub_ctypename);
    }
    else if (str_ctypename.rfind("rRef:", 0) == 0) {
        gname sub_ctypename(str_ctypename.substr(sizeof("rRef:") - 1));
        return build_prop_creator<CRRefProperty>(sub_ctypename);
    }
    else if (str_ctypename.rfind("raRef:", 0) == 0) {
        gname sub_ctypename(str_ctypename.substr(sizeof("raRef:") - 1));
        return build_prop_creator<CRaRefProperty>(sub_ctypename);
    }
    else if (str_ctypename == "Bool") {
        return build_prop_creator<CBoolProperty>();
    }
    else if (str_ctypename == "Uint8") {
        return build_prop_creator<CIntProperty>(EIntKind::U8);
    }
    else if (str_ctypename == "Int8") {
        return build_prop_creator<CIntProperty>(EIntKind::I8);
    }
    else if (str_ctypename == "Uint16") {
        return build_prop_creator<CIntProperty>(EIntKind::U16);
    }
    else if (str_ctypename == "Int16") {
        return build_prop_creator<CIntProperty>(EIntKind::I16);
    }
    else if (str_ctypename == "Uint32") {
        return build_prop_creator<CIntProperty>(EIntKind::U32);
    }
    else if (str_ctypename == "Int32") {
        return build_prop_creator<CIntProperty>(EIntKind::I32);
    }
    else if (str_ctypename == "Uint64") {
        return build_prop_creator<CIntProperty>(EIntKind::U64);
    }
    else if (str_ctypename == "Int64") {
        return build_prop_creator<CIntProperty>(EIntKind::I64);
    }
    else if (str_ctypename == "Float") {
        return build_prop_creator<CFloatProperty>();
    }
    else if (str_ctypename == "TweakDBID") {
        return build_prop_creator<CTweakDBIDProperty>();
    }
    else if (str_ctypename == "CName") {
        return build_prop_creator<CNameProperty>();
    }
    else if (str_ctypename == "CRUID") {
        return build_prop_creator<CCRUIDProperty>();
    }
    else if (str_ctypename == "NodeRef") {
        return build_prop_creator<CNodeRefProperty>();
    }
    else if (CEnum_resolver::get().is_registered(gname(str_ctypename))) {
        return build_prop_creator<CEnumProperty>(ctypename);
    }
    else if (str_ctypename == "gameSavedStatsData") {
        return build_prop_creator<CObjectProperty>(ctypename);
    }

    if (str_ctypename.find(':') == std::string::npos) {
        return build_prop_creator<CObjectProperty>(ctypename);
    }

    return build_prop_creator<CUnknownProperty>(ctypename);
}

} // namespace redx
