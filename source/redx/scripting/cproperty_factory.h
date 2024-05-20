#pragma once
#include <string>

#include <redx/scripting/CStringPool.h>
#include <redx/scripting/fwd.h>
#include <redx/scripting/iproperty.h>

namespace redx {

class CPropertyFactory {
private:
    CPropertyFactory() {
        // static init here
    }

    ~CPropertyFactory() {
    }

public:
    CPropertyFactory(const CPropertyFactory&)            = delete;
    CPropertyFactory& operator=(const CPropertyFactory&) = delete;

    static CPropertyFactory& get() {
        static CPropertyFactory s;
        return s;
    }

public:
    static std::function<CPropertyUPtr(CPropertyOwner*)> get_creator(gname ctypename);
};

} // namespace redx
