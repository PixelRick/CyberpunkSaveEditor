#pragma once
#include <array>
#include <exception>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#ifndef DISABLE_CP_IMGUI_WIDGETS
#    include <appbase/extras/cpp_imgui.hpp>
#    include <appbase/extras/imgui_stdlib.h>
#    include <appbase/widgets/list_widget.hpp>
#endif

#include <redx/core.hpp>
#include <redx/csav/node.h>
#include <redx/csav/serializers.h>
#include <redx/ctypes.hpp>

#include <redx/scripting/CStringPool.h>
#include <redx/scripting/csystem_serctx.h>

namespace redx {

enum class EPropertyKind {
    None,
    Unknown,
    Bool,
    Integer,
    Float,
    Double,
    Combo,
    Array,
    DynArray,
    Handle,
    RaRef,
    Object,
    TweakDBID,
    CName,
    CRUID,
    NodeRef
};

static inline std::string_view property_kind_to_display_name(EPropertyKind prop_kind) {
    switch (prop_kind) {
    case EPropertyKind::None:     return "None";
    case EPropertyKind::Unknown:  return "Unknown";
    case EPropertyKind::Bool:     return "Bool";
    case EPropertyKind::Integer:  return "Integer";
    case EPropertyKind::Float:    return "Float";
    case EPropertyKind::Double:   return "Double";
    case EPropertyKind::Combo:    return "Combo";
    case EPropertyKind::Array:    return "Array";
    case EPropertyKind::DynArray: return "DynArray";
    case EPropertyKind::Handle:   return "Handle";
    case EPropertyKind::Object:   return "Object";
    }
    return "Invalid";
}

enum class EPropertyEvent {
    data_edited,
    data_serialized_in
};

struct CPropertyOwner {
    virtual ~CPropertyOwner()                                                  = default;
    virtual void on_cproperty_event(const CProperty& prop, EPropertyEvent evt) = 0;
};

class CProperty {
    CPropertyOwner* m_owner;
    EPropertyKind   m_kind;
    bool            m_is_unskippable         = false;
    bool            m_is_freshly_constructed = true;

protected:
    CProperty(CPropertyOwner* owner, EPropertyKind kind)
        : m_owner(owner)
        , m_kind(kind) {
    }

public:
    virtual ~CProperty() = default;

    CPropertyOwner* owner() const {
        return m_owner;
    }
    EPropertyKind kind() const {
        return m_kind;
    }

    virtual gname ctypename() const = 0;

    bool is_skippable_in_serialization() const {
        return !m_is_unskippable && (m_is_freshly_constructed || has_default_value());
    }

    bool has_construction_value() const {
        return m_is_freshly_constructed;
    }

    // todo: dump them...
    virtual bool has_default_value() const {
        return false;
    }

    // serialization

protected:
    // when called, a data_modified event is sent automatically by base class' serialize_in(..)
    virtual bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) = 0;

public:
    bool serialize_in(std::istream& is, CSystemSerCtx& serctx) {
        bool ok = serialize_in_impl(is, serctx);
        post_cproperty_event(EPropertyEvent::data_serialized_in);
        return ok;
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const = 0;

    // gui (define DISABLE_CP_IMGUI_WIDGETS to disable implementations)

protected:
    // when returns true, a data_modified event is sent automatically by base class' imgui_widget(..)
    [[nodiscard]] virtual bool imgui_widget_impl(const char* label, bool editable) {
#ifndef DISABLE_CP_IMGUI_WIDGETS
        ImGui::Text("widget not implemented");
        return false;
#endif
    }

public:
    virtual bool imgui_is_one_liner() {
        return true;
    }

    static inline bool imgui_show_skipped = true;

    [[nodiscard]] bool imgui_widget(const char* label, bool editable) {
        //if (imgui_show_skipped && is_skippable_in_serialization())
        //  ImGui::Text("(default, may be skipped during serialization)");
        if (imgui_show_skipped && (is_skippable_in_serialization())) {
            ImGui::Text("(!)");
            ImGui::SameLine();
        }
        bool modified = imgui_widget_impl(label, editable);
        if (modified)
            post_cproperty_event(EPropertyEvent::data_edited);
        return modified;
    }

    void imgui_widget(const char* label) const {
        std::ignore = const_cast<CProperty*>(this)->imgui_widget(label, false);
    }

    // events

protected:
    void post_cproperty_event(EPropertyEvent evt) const {
        if (m_owner)
            m_owner->on_cproperty_event(*this, evt);
        auto nc_this = const_cast<CProperty*>(this);
        if (evt == EPropertyEvent::data_edited) {
            nc_this->m_is_freshly_constructed = false;
            nc_this->m_is_unskippable         = false;
        }
        else if (evt == EPropertyEvent::data_serialized_in) {
            nc_this->m_is_unskippable = true;
        }
    }
};

//------------------------------------------------------------------------------
// DEFAULT
//------------------------------------------------------------------------------

class CUnknownProperty : public CProperty {
protected:
    gname             m_ctypename;
    std::vector<char> m_data;

public:
    CUnknownProperty(CPropertyOwner* owner, gname ctypename)
        : CProperty(owner, EPropertyKind::Unknown)
        , m_ctypename(ctypename) {
    }

    ~CUnknownProperty() override = default;

public:
    const std::vector<char>& raw_data() const {
        return m_data;
    };

    // overrides

    gname ctypename() const override {
        return m_ctypename;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        std::streampos beg = is.tellg();
        is.seekg(0, std::ios_base::end);
        const size_t size = (size_t)(is.tellg() - beg);
        is.seekg(beg);
        m_data.resize(size);
        is.read(m_data.data(), size);
        //std::istream_iterator<char> it(is), end;
        //m_data.assign(it, end);
        return is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        os.write(m_data.data(), m_data.size());
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        bool modified = false;

        ImGui::BeginChild(label, ImVec2(0, 80), true, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("unsupported ctypename: %s", ctypename().c_str());
        ImGui::Text("data size: %08X", m_data.size());
        if (m_data.size() > 50)
            ImGui::Text("data: %s...", bytes_to_hex(m_data.data(), 50).c_str());
        else
            ImGui::Text("data: %s", bytes_to_hex(m_data.data(), m_data.size()).c_str());

        if (ImGui::Button("copy content to clipboard as hex")) {
            ImGui::SetClipboardText(bytes_to_hex(m_data.data(), m_data.size()).c_str());
        }

        ImGui::EndChild();

        return modified;
    }

#endif
};

} // namespace redx
