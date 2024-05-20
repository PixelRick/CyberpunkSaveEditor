#pragma once
#include <inttypes.h>

#include <redx/core.hpp>
#include <redx/ctypes.hpp>
#include <redx/scripting/csystem.h>

#include <redx/csav/node.h>
#include <redx/csav/serializers.h>

namespace redx::csav {

struct CGenericSystem : node_serializable {
    std::shared_ptr<const node_t> m_raw; // temporary
    std::string                   m_node_name;
    CSystem                       m_sys;

public:
    CGenericSystem() = default;

    const CSystem& system() const {
        return m_sys;
    }
    CSystem& system() {
        return m_sys;
    }

    std::string node_name() const override {
        return m_node_name;
    }

    bool
    from_node_impl(const std::shared_ptr<const node_t>& node, const version& version) override {
        if (!node)
            return false;

        m_raw       = node;
        m_node_name = node->name();

        node_reader reader(node, version);

        if (!m_sys.serialize_in(reader, version))
            return false;

        return reader.at_end();
    }

    std::shared_ptr<const node_t> to_node_impl(const version& version) const override {
        node_writer writer(version);

        if (!m_sys.serialize_out(writer))
            return nullptr;

        return writer.finalize(node_name());
    }
};

} // namespace redx::csav
