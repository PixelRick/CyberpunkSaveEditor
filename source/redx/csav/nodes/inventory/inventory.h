#pragma once
#include <iostream>
#include <list>

#include <redx/core.hpp>
#include <redx/ctypes.hpp>

#include <redx/csav/node.h>
#include <redx/csav/nodes/inventory/itemData.h>
#include <redx/csav/serializers.h>

namespace redx::csav {

struct SubInventory {
    uint64_t             uid = 0;
    std::list<CItemData> items;
};

struct CInventory : public node_serializable {
    std::list<SubInventory> m_subinvs;

    std::string node_name() const override {
        return "inventory";
    }

    bool
    from_node_impl(const std::shared_ptr<const node_t>& node, const version& version) override {
        if (!node)
            return false;

        node_reader reader(node, version);

        try {
            uint32_t inventory_cnt = 0;
            reader >> cbytes_ref(inventory_cnt);
            m_subinvs.resize(inventory_cnt);

            for (auto& subinv : m_subinvs) {
                reader >> cbytes_ref(subinv.uid);

                uint32_t items_cnt = 0;
                reader >> cbytes_ref(items_cnt);

                subinv.items.resize(items_cnt);
                for (auto& entry : subinv.items) {
                    CItemID iid = {};
                    reader >> cbytes_ref(iid.nameid.as_u64);
                    reader >> cbytes_ref(iid.uk.uk4);
                    if (version.v1 >= 97) {
                        reader >> cbytes_ref(iid.uk.uk1);
                        if (version.v1 >= 190) {
                            reader >> cbytes_ref(iid.uk.uk2);
                            if (version.v1 >= 221) {
                                reader >> cbytes_ref(iid.uk.uk5);
                            }
                        }
                    }

                    auto item_node = reader.read_child("itemData");
                    if (!item_node)
                        return false; // todo: don't leave this in this state

                    if (!entry.from_node(item_node, version))
                        return false;

                    if (!reader.good())
                        return false;
                }
            }
        }
        catch (std::ios::failure&) {
            return false;
        }

        return reader.at_end();
    }

    std::shared_ptr<const node_t> to_node_impl(const version& version) const override {
        node_writer writer(version);

        uint32_t inventory_cnt = (uint32_t)m_subinvs.size();
        writer.write((char*)&inventory_cnt, 4);

        for (auto& subinv : m_subinvs) {
            writer.write((char*)&subinv.uid, 8);

            uint32_t items_cnt = (uint32_t)subinv.items.size();
            writer.write((char*)&items_cnt, 4);

            for (const auto& entry : subinv.items) {
                auto item_node = entry.to_node(version);
                if (!item_node)
                    return nullptr; // todo: don't leave this in this state

                writer << entry.iid;
                writer.write_child(item_node);
            }
        }

        return writer.finalize(node_name());
    }
};

} // namespace redx::csav
