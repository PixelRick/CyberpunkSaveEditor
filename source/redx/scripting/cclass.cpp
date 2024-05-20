#pragma once
#include <Windows.h>

#include <redx/scripting/cclass.h>

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

/* example:
"AIMeleeAttackCommand": {
  "ctypename": "AIMeleeAttackCommand",
  "parent": "AICombatRelatedCommand",
  "props": [
    {
      "ctypename": "NodeRef",
      "gname": "targetOverrideNodeRef"
    },
    {
      "ctypename": "gameEntityReference",
      "gname": "targetOverridePuppetRef"
    },
    {
      "ctypename": "Float",
      "gname": "duration"
    }
  ]
},

*/

namespace redx {

static void to_json(nlohmann::json& j, const CFieldDesc& p) {
    j = {
        {     "name",      p.name.c_str()},
        {"ctypename", p.ctypename.c_str()}
    };
}

static void from_json(const nlohmann::json& j, CFieldDesc& p) {
    p.name      = gname(j.at("name").get<std::string>());
    p.ctypename = gname(j.at("ctypename").get<std::string>());
}

// isn't safe against circular dependencies.. but shouldn't happen anyway
CObjectBPSPtr read_class_bp(
    std::unordered_map<gname, CObjectBPSPtr>& classmap,
    nlohmann::json&                           j,
    nlohmann::json::iterator                  j_it) {
    gname ctypename(j_it.key());

    auto it = classmap.find(ctypename);
    if (it != classmap.end())
        return it->second;

    auto& cdef = j_it.value();

    CObjectBPSPtr parent = nullptr;
    if (cdef.find("parent") != cdef.end()) {
        auto parent_name = cdef["parent"].get<std::string>();
        auto parent_it   = j.find(parent_name);
        if (parent_it == j.end()) {
            auto err = fmt::format(
                "Incomplete DB, {} is missing parent def {}", ctypename.c_str(), parent_name);
            MessageBoxA(0, err.c_str(), "CObjectBPList Error", 0);
            throw std::runtime_error(err);
        }
        parent = read_class_bp(classmap, j, parent_it);
    }

    std::vector<CFieldDesc> fdescs;
    cdef["props"].get_to(fdescs);

    auto new_bp = std::make_shared<CObjectBP>(ctypename, parent, fdescs);
    if (parent)
        parent->add_child(new_bp);

    classmap.emplace(ctypename, new_bp);

    return new_bp;
}

CObjectBPList::CObjectBPList() {
    std::ifstream ifs;
    ifs.open("db/CObjectBPs.json");
    if (ifs.is_open()) {
        nlohmann::json db;
        try {
            ifs >> db;
            for (nlohmann::json::iterator it = db.begin(); it != db.end(); ++it) {
                read_class_bp(m_classmap, db, it);
            }
        }
        catch (std::exception& e) {
            std::wostringstream oss;
            oss << L"db/CObjectBPs.json has unexpected content" << std::endl;
            oss << e.what();
            MessageBox(0, oss.str().c_str(), L"corrupt resource file", 0);
        }
    }
    else {
        MessageBox(0, L"db/CEnums.json is missing", L"couldn't open resource file", 0);
    }
}

CObjectBPList::~CObjectBPList() {
}

} // namespace redx
