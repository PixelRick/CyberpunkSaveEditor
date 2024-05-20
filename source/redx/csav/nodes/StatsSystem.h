#pragma once
#include <inttypes.h>

#include <redx/core.hpp>
#include <redx/ctypes.hpp>
#include <redx/scripting/cproperty.h>
#include <redx/scripting/csystem.h>

#include <redx/csav/node.h>
#include <redx/csav/serializers.h>

namespace redx::csav {

struct CStats : public node_serializable, public ObjectListener {
    std::shared_ptr<const node_t> m_raw; // temporary
    CSystem                       m_sys;

    CObjectSPtr                               m_mapstruct = nullptr;
    std::unordered_map<uint32_t, CObjectSPtr> m_seed_to_stats_obj_map;

public:
    CStats() = default;

    const CSystem& system() const {
        return m_sys;
    }
    CSystem& system() {
        return m_sys;
    }

public:
    // tools

    // todo: add a field* param to this listener, so that we can do things depending on which property has been edited
    void on_cobject_event(const CObject& obj, ObjectEventType evt) override {
        if (m_mapstruct.get() == &obj)
            update_stats_stuff();
    }

    bool update_stats_stuff() {
        // this isn't editor breaking, so we return true if it fails

        m_seed_to_stats_obj_map.clear();

        if (m_sys.objects().empty())
            return false;

        // update m_mapstruct
        if (m_mapstruct)
            m_mapstruct->remove_listener(this);

        m_mapstruct = m_sys.objects()[0];
        if (!m_mapstruct)
            return true;

        m_mapstruct->add_listener(this);

        // update seed->stats map

        static gname gn_values = "values"_gndef;
        static gname gn_seed   = "seed"_gndef;

        auto mapvalues           = m_mapstruct->get_prop(gn_values);
        auto m_stats_values_prop = dynamic_cast<CDynArrayProperty*>(mapvalues);
        if (!m_stats_values_prop)
            return true;

        // iterate over gameSavedStatsData objects
        for (auto& it : *m_stats_values_prop) {
            CObjectProperty* objprop = dynamic_cast<CObjectProperty*>(it.get());
            if (!objprop)
                continue;
            auto obj = objprop->obj();

            auto seedprop = dynamic_cast<CIntProperty*>(obj->get_prop(gn_seed));
            if (!seedprop)
                continue;
            uint32_t seed = seedprop->u32();
            m_seed_to_stats_obj_map.emplace(seed, obj);
        }

        return true;
    }

    CProperty* get_modifiers_prop(uint32_t seed) {
        auto stats = m_seed_to_stats_obj_map.find(seed);
        if (stats == m_seed_to_stats_obj_map.end())
            return nullptr;

        static gname gn_statModifiers = "statModifiers"_gndef;
        auto         modsarray =
            dynamic_cast<CDynArrayProperty*>(stats->second->get_prop(gn_statModifiers));
        if (!modsarray)
            return nullptr;

        return modsarray;
    }

protected:
    CObjectSPtr add_new_modifier(CProperty* modifiers, gname name) {
        auto mods = dynamic_cast<CDynArrayProperty*>(modifiers);
        if (!mods)
            return nullptr;
        auto new_handle = dynamic_cast<CHandleProperty*>(mods->emplace(mods->begin())->get());
        if (!new_handle)
            return nullptr; //damnit
        auto new_obj = std::make_shared<CObject>(name, m_sys.is_using_blueprints());
        new_handle->set_obj(new_obj);
        return new_obj;
    }

public:
    CObjectSPtr add_combined_stats(CProperty* modifiers) {
        static gname gn_gameCombinedStatModifierData =
            "gameCombinedStatModifierData_Deprecated"_gndef;
        return add_new_modifier(modifiers, gn_gameCombinedStatModifierData);
    }

    CObjectSPtr add_curve_stats(CProperty* modifiers) {
        static gname gn_gameCurveStatModifierData = "gameCurveStatModifierData_Deprecated"_gndef;
        return add_new_modifier(modifiers, gn_gameCurveStatModifierData);
    }

    CObjectSPtr add_constant_stats(CProperty* modifiers) {
        static gname gn_gameConstantStatModifierData =
            "gameConstantStatModifierData_Deprecated"_gndef;
        static gname gn_modifierType = "modifierType"_gndef;
        static gname gn_statType     = "statType"_gndef;
        static gname gn_value        = "value"_gndef;

        auto a = add_new_modifier(modifiers, gn_gameConstantStatModifierData);
        a->get_prop_cast<CEnumProperty>(gn_modifierType)->set_value_by_idx(0);
        a->get_prop_cast<CEnumProperty>(gn_statType)->set_value_by_idx(0);
        a->get_prop_cast<CFloatProperty>(gn_value)->set_value(1.0f);
        return a;
    }

public:
    std::string node_name() const override {
        return "StatsSystem";
    }

    bool from_node_impl(const std::shared_ptr<const node_t>& node, const version& ver) override {
        if (!node)
            return false;

        m_raw = node;

        node_reader reader(node, ver);

        // todo: catch exception at upper level to display error
        if (!m_sys.serialize_in(reader, ver))
            return false;

        if (!reader.at_end())
            return false;

        update_stats_stuff();

        return true;
    }

    std::shared_ptr<const node_t> to_node_impl(const version& version) const override {
        node_writer writer(version);

        try {
            if (!m_sys.serialize_out(writer))
                return nullptr;
        }
        catch (std::exception&) {
            return nullptr;
        }

        return writer.finalize(node_name());
    }
};

} // namespace redx::csav
