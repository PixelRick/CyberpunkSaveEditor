#pragma once
#include <array>
#include <exception>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

// todo: move the ui part out of this lib
#include <appbase/widgets/redx.hpp>
#include <imgui/extras/ImGuizmo.h>

#include <redx/core.hpp>
#include <redx/csav/serializers.h>
#include <redx/ctypes.hpp>

#include <redx/serialization/resids_blob.hpp>

#include <redx/scripting/cobject.h>
#include <redx/scripting/cproperty_factory.h>
#include <redx/scripting/cstringpool.h>
#include <redx/scripting/fwd.h>
#include <redx/scripting/iproperty.h>

//------------------------------------------------------------------------------
// BOOL
//------------------------------------------------------------------------------

class CBoolProperty : public CProperty {
public:
    CBoolProperty(CPropertyOwner* owner)
        : CProperty(owner, EPropertyKind::Bool) {
    }

    ~CBoolProperty() override = default;

    // overrides

    gname ctypename() const override {
        static gname sname("Bool");
        return sname;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        char val = 0;
        is >> cbytes_ref(val);
        m_value = !!val;
        return is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        char val = m_value ? 1 : 0;
        os.write(&val, 1);
        return true;
    }

    bool value() const {
        return m_value;
    }

    void value(bool value) {
        // whatever happens.. because we don't really know the default values
        post_cproperty_event(EPropertyEvent::data_edited);
        m_value = value;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        // no disabled checkbox in imgui atm
        if (!editable) {
            ImGui::Text(label);
            ImGui::SameLine();
            ImGui::Text(m_value ? ": true" : ": false");
            return false;
        }

        return ImGui::Checkbox(label, &m_value);
    }

#endif

private:
    bool m_value = false;
};

//------------------------------------------------------------------------------
// INTEGER
//------------------------------------------------------------------------------

enum class EIntKind {
    U8,
    I8,
    U16,
    I16,
    U32,
    I32,
    U64,
    I64,
};

class CIntProperty : public CProperty {
protected:
    union {
        uint8_t  u8;
        int8_t   i8;
        uint16_t u16;
        int16_t  i16;
        uint32_t u32;
        int32_t  i32;
        uint64_t u64;
        int64_t  i64 = 0;
    } m_value;

protected:
    const EIntKind m_int_kind;
    const size_t   m_int_size;
    gname          m_ctypename;

public:
    CIntProperty(CPropertyOwner* owner, EIntKind int_kind)
        : CProperty(owner, EPropertyKind::Integer)
        , m_int_kind(int_kind)
        , m_int_size(int_size(int_kind))
        , m_ctypename(int_ctypename(int_kind)) {
    }

    ~CIntProperty() override = default;

public:
    EIntKind int_kind() const {
        return m_int_kind;
    }

public:
    static inline size_t int_size(EIntKind int_kind) {
        switch (int_kind) {
        case EIntKind::U8:
        case EIntKind::I8:  return 1;
        case EIntKind::U16:
        case EIntKind::I16: return 2;
        case EIntKind::U32:
        case EIntKind::I32: return 4;
        case EIntKind::U64:
        case EIntKind::I64: return 8;
        default:            break;
        }
        throw std::domain_error("unknown EIntKind");
    }

    static inline std::string int_ctypename(EIntKind int_kind) {
        switch (int_kind) {
        case EIntKind::U8:  return "Uint8";
        case EIntKind::I8:  return "Int8";
        case EIntKind::U16: return "Uint16";
        case EIntKind::I16: return "Int16";
        case EIntKind::U32: return "Uint32";
        case EIntKind::I32: return "Int32";
        case EIntKind::U64: return "Uint64";
        case EIntKind::I64: return "Int64";
        default:            break;
        }
        throw std::domain_error("unknown EIntKind");
    };

    uint64_t u64() const {
        return m_value.u64;
    }
    uint32_t u32() const {
        return m_value.u32;
    }
    uint16_t u16() const {
        return m_value.u16;
    }
    uint8_t u8() const {
        return m_value.u8;
    }

    // todo: template this class..

    void u64(uint64_t value) {
        // whatever happens.. because we don't really know the default values
        post_cproperty_event(EPropertyEvent::data_edited);
        m_value.u64 = value;
    }

    void u32(uint32_t value) {
        // whatever happens.. because we don't really know the default values
        post_cproperty_event(EPropertyEvent::data_edited);
        m_value.u32 = value;
    }

    void u16(uint16_t value) {
        // whatever happens.. because we don't really know the default values
        post_cproperty_event(EPropertyEvent::data_edited);
        m_value.u16 = value;
    }

    void u8(uint8_t value) {
        // whatever happens.. because we don't really know the default values
        post_cproperty_event(EPropertyEvent::data_edited);
        m_value.u8 = value;
    }

    // overrides

    gname ctypename() const override {
        return m_ctypename;
    }

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        is.read((char*)&m_value.u64, m_int_size);
        return is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        os.write((char*)&m_value.u64, m_int_size);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        //auto label = ctypename() + "##CIntProperty";
        ImGuiDataType dtype = ImGuiDataType_U64;
        auto          dfmt  = "%016llX";

        switch (m_int_kind) {
        case EIntKind::U8:
            dtype = ImGuiDataType_U8;
            dfmt  = "%02X";
            break;
        case EIntKind::I8:
            dtype = ImGuiDataType_U8;
            dfmt  = "%02X";
            break;
        case EIntKind::U16:
            dtype = ImGuiDataType_U16;
            dfmt  = "%04X";
            break;
        case EIntKind::I16:
            dtype = ImGuiDataType_U16;
            dfmt  = "%04X";
            break;
        case EIntKind::U32:
            dtype = ImGuiDataType_U32;
            dfmt  = "%08X";
            break;
        case EIntKind::I32:
            dtype = ImGuiDataType_U32;
            dfmt  = "%08X";
            break;
        case EIntKind::U64:
            dtype = ImGuiDataType_U64;
            dfmt  = "%016llX";
            break;
        case EIntKind::I64:
            dtype = ImGuiDataType_U64;
            dfmt  = "%016llX";
            break;
        default: break;
        }

        return ImGui::InputScalar(
            label,
            dtype,
            &m_value.u64,
            0,
            0,
            dfmt,
            ImGuiInputTextFlags_CharsHexadecimal | (editable ? 0 : ImGuiInputTextFlags_ReadOnly));
    }

#endif
};

//------------------------------------------------------------------------------
// FLOATING POINT
//------------------------------------------------------------------------------

class CFloatProperty : public CProperty {
protected:
    float m_value = 0.f;

public:
    CFloatProperty(CPropertyOwner* owner)
        : CProperty(owner, EPropertyKind::Float) {
    }

    ~CFloatProperty() override = default;

public:
    // overrides

    gname ctypename() const override {
        static gname sname("Float");
        return sname;
    };

    float value() const {
        return m_value;
    }

    void set_value(float value) {
        // whatever happens.. because we don't really know the default values
        post_cproperty_event(EPropertyEvent::data_edited);
        m_value = value;
    }

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        is >> cbytes_ref(m_value);
        return is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        os << cbytes_ref(m_value);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        return ImGui::InputFloat(
            label, &m_value, 0, 0, "%.3f", editable ? 0 : ImGuiInputTextFlags_ReadOnly);
    }

#endif
};

//------------------------------------------------------------------------------
// ARRAY (fixed len)
//------------------------------------------------------------------------------

class CArrayProperty : public CProperty, public CPropertyOwner {
public:
    using container_type = std::vector<CPropertyUPtr>;
    using iterator       = container_type::iterator;
    using const_iterator = container_type::const_iterator;

protected:
    container_type m_elts;

protected:
    // with a pointer to System in CProperty we could use
    // CName ids too.. but that's for later
    gname                                         m_elt_ctypename;
    gname                                         m_typename;
    std::function<CPropertyUPtr(CPropertyOwner*)> m_elt_create_fn;

public:
    CArrayProperty(CPropertyOwner* owner, gname elt_ctypename, size_t size)
        : CProperty(owner, EPropertyKind::DynArray)
        , m_elt_ctypename(elt_ctypename)
        , m_typename(fmt::format("[{}]{}", size, elt_ctypename.strv())) {
        m_elts.resize(size);
        auto& factory   = CPropertyFactory::get();
        m_elt_create_fn = factory.get_creator(elt_ctypename);
        for (auto& elt : m_elts) {
            // todo: use clone on first instance..
            elt = m_elt_create_fn(this);
        }
    }

public:
    const container_type& elts() const {
        return m_elts;
    }

    const_iterator cbegin() const {
        return m_elts.cbegin();
    }
    const_iterator cend() const {
        return m_elts.cend();
    }

    const_iterator begin() const {
        return cbegin();
    }
    const_iterator end() const {
        return cend();
    }

    iterator begin() {
        return m_elts.begin();
    }
    iterator end() {
        return m_elts.end();
    }

    // overrides

    gname ctypename() const override {
        return m_typename;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        size_t start_pos = is.tellg();

        uint32_t uk = 0;
        is >> cbytes_ref(uk);
        if (uk != m_elts.size())
            throw std::logic_error("CArrayProperty: false assumption #1. please open an issue");

        for (auto& elt : m_elts) {
            if (elt->kind() == EPropertyKind::Unknown)
                return false;
            if (!elt->serialize_in(is, serctx))
                return false;
        }

        size_t end_pos = is.tellg();
        serctx.log(fmt::format(
            "serialized_in {} in {} bytes", this->ctypename(), (size_t)(end_pos - start_pos)));

        return is.good();
    }

    virtual [[nodiscard]] bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        size_t start_pos = os.tellp();

        uint32_t uk = (uint32_t)m_elts.size();
        os << cbytes_ref(uk);

        for (auto& elt : m_elts) {
            if (!elt->serialize_out(os, serctx))
                return false;
        }

        size_t end_pos = os.tellp();
        serctx.log(fmt::format(
            "serialized_in {} in {} bytes",
            this->ctypename().c_str(),
            (size_t)(end_pos - start_pos)));

        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        bool modified = false;

        static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;

        ImVec2 size = ImVec2(-FLT_MIN, std::min(400.f, ImGui::GetContentRegionAvail().y));
        if (ImGui::BeginTable(label, 2, tbl_flags, size)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("idx", ImGuiTableColumnFlags_WidthFixed, 30.f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            size_t idx    = 0;
            int    to_rem = -1;
            for (auto& elt : m_elts) {
                auto lbl = fmt::format("{:03d}", idx);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text(lbl.c_str());
                if (editable && ImGui::BeginPopupContextItem("item context menu")) {
                    if (ImGui::Selectable("^ delete"))
                        to_rem = (int)idx;
                    ImGui::EndPopup();
                }

                ImGui::TableNextColumn();

                modified |= elt->imgui_widget(lbl.c_str(), editable);

                ++idx;
            }

            if (to_rem >= 0) {
                m_elts.erase(m_elts.begin() + to_rem);
                modified = true;
            }

            ImGui::EndTable();
        }

        return modified;
    }

    bool imgui_is_one_liner() override {
        return false;
    }

#endif

    // child events

    void on_cproperty_event(const CProperty& prop, EPropertyEvent evt) override {
        post_cproperty_event(evt);
    }
};

//------------------------------------------------------------------------------
// DYN ARRAY
//------------------------------------------------------------------------------

class CDynArrayProperty : public CProperty, public CPropertyOwner {
public:
    using container_type = std::vector<CPropertyUPtr>;
    using iterator       = container_type::iterator;
    using const_iterator = container_type::const_iterator;

protected:
    container_type m_elts;

protected:
    // with a pointer to System in CProperty we could use
    // CName ids too.. but that's for later
    gname                                         m_elt_ctypename;
    gname                                         m_ctypename;
    std::function<CPropertyUPtr(CPropertyOwner*)> m_elt_create_fn;

public:
    CDynArrayProperty(CPropertyOwner* owner, gname elt_ctypename)
        : CProperty(owner, EPropertyKind::DynArray)
        , m_elt_ctypename(elt_ctypename)
        , m_ctypename(std::string("array:") + elt_ctypename.c_str()) {
        auto& factory   = CPropertyFactory::get();
        m_elt_create_fn = factory.get_creator(elt_ctypename);
    }

public:
    const container_type& elts() const {
        return m_elts;
    }

    const_iterator cbegin() const {
        return m_elts.cbegin();
    }
    const_iterator cend() const {
        return m_elts.cend();
    }

    const_iterator begin() const {
        return cbegin();
    }
    const_iterator end() const {
        return cend();
    }

    iterator begin() {
        return m_elts.begin();
    }
    iterator end() {
        return m_elts.end();
    }

    iterator emplace(const_iterator _where) {
        auto it = m_elts.emplace(_where, m_elt_create_fn(this));
        post_cproperty_event(EPropertyEvent::data_edited);
        return it;
    }

    // overrides

    gname ctypename() const override {
        return m_ctypename;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        m_elts.clear();

        uint32_t cnt = 0;
        is >> cbytes_ref(cnt);

        auto& factory = CPropertyFactory::get();
        m_elts.resize(cnt);

        for (auto& elt : m_elts) {
            elt = m_elt_create_fn(this);
            if (elt->kind() == EPropertyKind::Unknown)
                return false;
            if (!elt->serialize_in(is, serctx))
                return false;
        }

        return is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        uint32_t cnt = (uint32_t)m_elts.size();
        os << cbytes_ref(cnt);

        for (auto& elt : m_elts) {
            if (!elt->serialize_out(os, serctx))
                return false;
        }

        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        bool modified = false;

        static ImGuiTableFlags tbl_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;

        ImVec2 size = ImVec2(-FLT_MIN, std::min(400.f, ImGui::GetContentRegionAvail().y));
        if (ImGui::BeginTable(label, 2, tbl_flags, size)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn(
                "idx", ImGuiTableColumnFlags_WidthFixed, editable ? 68.f : 28.f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            int to_rem = -1;
            int to_ins = -1;
            for (size_t idx = 0; idx < m_elts.size(); ++idx) {
                scoped_imgui_id _sii((int)idx);

                auto& elt = m_elts[idx];

                auto lbl = fmt::format("{:03d}", idx);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::Text(lbl.c_str());
                if (editable) {
                    ImGui::SameLine();
                    if (ImGui::ArrowButton("up", ImGuiDir_Up)) {
                        if (idx > 0) {
                            size_t prev = idx - 1;
                            std::swap(m_elts[idx], m_elts[prev]);
                            modified = true;
                        }
                    }
                    ImGui::PushStyleVar(
                        ImGuiStyleVar_ItemSpacing, ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
                    ImGui::SameLine();
                    if (ImGui::ArrowButton("down", ImGuiDir_Down)) {
                        if (idx + 1 < m_elts.size()) {
                            size_t next = idx + 1;
                            std::swap(m_elts[idx], m_elts[next]);
                            modified = true;
                        }
                    }
                    ImGui::PopStyleVar();
                    if (editable && ImGui::SmallButton("delete"))
                        to_rem = (int)idx;

                    if (editable && ImGui::SmallButton("insert"))
                        to_ins = (int)idx;
                }

                ImGui::TableNextColumn();
                modified |= elt->imgui_widget(lbl.c_str(), editable);
            }

            if (m_elts.empty()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("insert"))
                    to_ins = 0;
            }

            ImGui::EndTable();

            if (to_rem >= 0) {
                auto remit = m_elts.begin() + to_rem;
                m_elts.erase(remit);
                modified = true;
            }

            if (to_ins >= 0) {
                auto insit = m_elts.begin() + to_ins;
                emplace(insit);
                modified = true;
            }
        }

        return modified;
    }

    bool imgui_is_one_liner() override {
        return false;
    }

#endif

    // child events

    void on_cproperty_event(const CProperty& prop, EPropertyEvent evt) override {
        post_cproperty_event(evt);
    }
};

//------------------------------------------------------------------------------
// OBJECT
//------------------------------------------------------------------------------

class CObjectProperty : public CProperty, public ObjectListener {
protected:
    CObjectSPtr m_object;
    gname       m_obj_ctypename;

public:
    CObjectProperty(CPropertyOwner* owner, gname obj_ctypename)
        : CProperty(owner, EPropertyKind::Object)
        , m_obj_ctypename(obj_ctypename) {
        m_object = std::make_shared<CObject>(m_obj_ctypename);
        m_object->add_listener(this);
    }

    ~CObjectProperty() override {
        m_object->remove_listener(this);
    }

public:
    CObjectSPtr obj() const {
        return m_object;
    }

    // overrides

    gname ctypename() const override {
        return m_obj_ctypename;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        return m_object->serialize_in(is, serctx) && is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        return m_object->serialize_out(os, serctx);
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        return m_object->imgui_widget(label, editable);
    }

    bool imgui_is_one_liner() override {
        static gname gn_WorldPosition = "WorldPosition"_gndef;
        static gname gn_Quaternion    = "Quaternion"_gndef;

        return m_obj_ctypename == gn_WorldPosition || m_obj_ctypename == gn_Quaternion;
    }

#endif

    // object events

    void on_cobject_event(const CObject& prop, ObjectEventType evt) override {
        post_cproperty_event(EPropertyEvent::data_edited);
    }
};

//------------------------------------------------------------------------------
// ENUM
//------------------------------------------------------------------------------

class CEnumProperty : public CProperty {
protected:
    gname                          m_enum_name;
    uint32_t                       m_bp_index = 0;
    gname                          m_val_name;
    CEnum_resolver::enum_desc_sptr m_enum_desc;

public:
    CEnumProperty(CPropertyOwner* owner, gname enum_name)
        : CProperty(owner, EPropertyKind::Combo)
        , m_enum_name(enum_name) {
        m_enum_desc   = CEnum_resolver::get().get_enum(enum_name);
        auto& members = m_enum_desc->members();
        if (members.size())
            m_val_name = members.at(m_bp_index).name();
        else
            m_val_name = gname("empty enum");
    }

    ~CEnumProperty() override = default;

public:
    bool has_default_value() const override {
        // disabled: we don't know them.. let's find out later if it's dumpable or not
        return false;
        //return m_bp_index == 0;
    }

    gname value_name() const {
        return m_val_name;
    }

    // returns true if gname exists
    bool set_value_by_name(gname name) {
        bool success = false;
        if (name != m_val_name) {
            auto& enum_members = m_enum_desc->members();
            for (size_t i = 0; i < enum_members.size(); ++i) {
                if (enum_members[i].name() == name) {
                    m_bp_index = (uint32_t)i;
                    m_val_name = name;
                    success    = true;
                    break;
                }
            }
        }
        // whatever happens.. because we don't really know the default values
        post_cproperty_event(EPropertyEvent::data_edited);
        return success;
    }

    bool set_value_by_idx(size_t idx) {
        bool  success      = false;
        auto& enum_members = m_enum_desc->members();
        if (idx < enum_members.size()) {
            m_bp_index = (uint32_t)idx;
            m_val_name = enum_members[idx].name();
            success    = true;
        }
        // whatever happens.. because we don't really know the default values
        post_cproperty_event(EPropertyEvent::data_edited);
        return success;
    }

    // overrides

    gname ctypename() const override {
        return m_enum_name;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        uint16_t strpool_idx = 0;
        is >> cbytes_ref(strpool_idx);
        if (strpool_idx >= serctx.strpool.size())
            return false;
        m_val_name         = serctx.strpool.at(strpool_idx).gstr();
        auto& enum_members = m_enum_desc->members();
        m_bp_index         = (uint32_t)enum_members.size();
        for (size_t i = 0; i < enum_members.size(); ++i) {
            if (enum_members[i].name() == m_val_name) {
                m_bp_index = (uint32_t)i;
                break;
            }
        }
        if (m_bp_index == enum_members.size()) {
            // TODO: Ensure the db is correct to avoid this. The value is unknown..
            enum_members.emplace_back(m_val_name, 0);
        }

        return is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        static gname gn_no_zero_name = "<no_zero_name>"_gndef;

        if (m_val_name == gn_no_zero_name)
            throw std::logic_error("enum value must be skipped, 0 has no gname");

        uint16_t strpool_idx = serctx.strpool.insert(m_val_name);
        os << cbytes_ref(strpool_idx);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    static bool enum_name_getter(void* data, int idx, const char** out_str) {
        auto& enum_members = *(std::vector<CEnum_member>*)data;
        if (idx < 0 || idx >= enum_members.size())
            return false;
        *out_str = enum_members[idx].name().c_str();
        return true;
    }

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        auto& enum_members = m_enum_desc->members();
        int   current_item = m_bp_index;

        auto current_enum_name = fmt::format("unknown enum value {}", current_item);
        if (current_item >= 0 && current_item < enum_members.size())
            current_enum_name = enum_members[current_item].name().string();
        else
            ImGui::Text(fmt::format("unknown enum value %d", current_item).c_str());

        if (editable)
            ImGui::BetterCombo(
                label,
                &current_item,
                &enum_name_getter,
                (void*)&enum_members,
                (int)enum_members.size());
        else
            ImGui::Text("%s: %s::%s", label, m_enum_name.c_str(), current_enum_name);

        if (current_item == m_bp_index || current_item < 0)
            return false;

        m_bp_index = (uint16_t)current_item;
        m_val_name = enum_members[m_bp_index].name();
        return true;
    }

#endif
};

//------------------------------------------------------------------------------
// TweakDBID
//------------------------------------------------------------------------------

class CTweakDBIDProperty : public CProperty {
public:
    TweakDBID m_id = {};

public:
    CTweakDBIDProperty(CPropertyOwner* owner)
        : CProperty(owner, EPropertyKind::TweakDBID) {
    }

    ~CTweakDBIDProperty() override = default;

public:
    // overrides

    gname ctypename() const override {
        static gname sname("TweakDBID");
        return sname;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        is >> cbytes_ref(m_id.as_u64);
        return is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        os << cbytes_ref(m_id.as_u64);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        return TweakDBID_widget::draw(m_id, label);
    }

#endif
};

//------------------------------------------------------------------------------
// CName
//------------------------------------------------------------------------------

class CNameProperty : public CProperty {
protected:
    CName m_id = {};

public:
    CNameProperty(CPropertyOwner* owner)
        : CProperty(owner, EPropertyKind::CName) {
    }

    ~CNameProperty() override = default;

public:
    // overrides

    gname ctypename() const override {
        static gname sname("CName");
        return sname;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        uint16_t strpool_idx = 0;
        is >> cbytes_ref(strpool_idx);
        if (!is.good() || strpool_idx >= serctx.strpool.size())
            return false;

        m_id = serctx.strpool.at(strpool_idx);
        return true;
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        uint16_t strpool_idx = serctx.strpool.insert(m_id);
        os << cbytes_ref(strpool_idx);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        return CName_widget::draw(m_id, label);
    }

#endif
};

//------------------------------------------------------------------------------
// rRef
//------------------------------------------------------------------------------

class CRRefProperty : public CProperty {
protected:
    gname m_base_ctypename;
    gname m_ctypename;
    cname m_path;

public:
    CRRefProperty(CPropertyOwner* owner, gname sub_ctypename)
        : CProperty(owner, EPropertyKind::RaRef)
        , m_base_ctypename(sub_ctypename)
        , m_ctypename(std::string("rRef:") + m_base_ctypename.c_str()) {
    }

    ~CRRefProperty() override = default;

public:
    // overrides

    gname ctypename() const override {
        return m_ctypename;
    }

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        uint16_t idx;
        is >> cbytes_ref(idx);
        auto rid = serctx.respool.at(idx);
        m_path   = rid.path;
        return true;
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        uint16_t idx = serctx.respool.insert(m_path, true);
        os << cbytes_ref(idx);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        return CName_widget::draw(m_path, label);
    }

#endif
};

//------------------------------------------------------------------------------
// raRef
//------------------------------------------------------------------------------

class CRaRefProperty : public CProperty {
protected:
    gname m_base_ctypename;
    gname m_ctypename;
    cname m_path;

public:
    CRaRefProperty(CPropertyOwner* owner, gname sub_ctypename)
        : CProperty(owner, EPropertyKind::RaRef)
        , m_base_ctypename(sub_ctypename)
        , m_ctypename(std::string("raRef:") + m_base_ctypename.c_str()) {
    }

    ~CRaRefProperty() override = default;

public:
    // overrides

    gname ctypename() const override {
        return m_ctypename;
    }

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        uint16_t idx;
        is >> cbytes_ref(idx);
        auto rid = serctx.respool.at(idx);
        m_path   = rid.path;
        return true;
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        uint16_t idx = serctx.respool.insert(m_path, false);
        os << cbytes_ref(idx);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        return CName_widget::draw(m_path, label);
    }

#endif
};

//------------------------------------------------------------------------------
// CRUID
//------------------------------------------------------------------------------

class CCRUIDProperty : public CProperty {
protected:
    uint64_t m_id = {};

public:
    CCRUIDProperty(CPropertyOwner* owner)
        : CProperty(owner, EPropertyKind::CRUID) {
    }

    ~CCRUIDProperty() override = default;

public:
    // overrides

    gname ctypename() const override {
        static gname sname("CRUID");
        return sname;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        is >> cbytes_ref(m_id);
        return is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        os << cbytes_ref(m_id);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        return ImGui::InputScalar(
            label,
            ImGuiDataType_U64,
            &m_id,
            0,
            0,
            "%016llX",
            ImGuiInputTextFlags_CharsHexadecimal | (editable ? 0 : ImGuiInputTextFlags_ReadOnly));
    }

#endif
};

//------------------------------------------------------------------------------
// HANDLE (pointers..)
//------------------------------------------------------------------------------

// for now, replacing the object is permissive, but the original obj_ctypename
// must be a base class of the new object class.
// todo: implement inheritance in the BP database
//
// also, base type may not be constructible, so a default type might be necessary..
//

class CHandleProperty : public CProperty, public ObjectListener {
protected:
    gname       m_base_ctypename;
    gname       m_ctypename;
    CObjectSPtr m_obj;

    uint32_t m_original_handle = 0;

public:
    CHandleProperty(CPropertyOwner* owner, gname sub_ctypename)
        : CProperty(owner, EPropertyKind::Handle)
        , m_base_ctypename(sub_ctypename)
        , m_ctypename(std::string("handle:") + m_base_ctypename.c_str()) {
        // let's not infinite loop...
        m_obj = nullptr; //std::make_shared<CObject>(m_base_ctypename);
    }

    ~CHandleProperty() override = default;

public:
    CObjectSPtr obj() const {
        return m_obj;
    }

    void set_obj(const CObjectSPtr& new_obj) {
        if (!new_obj) // no!
            return;
        if (m_obj)
            m_obj->remove_listener(this);
        new_obj->add_listener(this);
        // assign
        m_obj = new_obj;
    }

    // overrides

    gname ctypename() const override {
        return m_ctypename;
    }

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        m_original_handle = 0;
        is >> cbytes_ref(m_original_handle);

        auto new_obj = serctx.from_handle(m_original_handle);
        set_obj(new_obj);

        return new_obj && is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        if (!m_obj)
            throw std::runtime_error("can't serialize_out a null handle");

        const_cast<CHandleProperty*>(this)->m_original_handle = serctx.to_handle(m_obj);
        os << cbytes_ref(m_original_handle);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        if (!m_obj) {
            ImGui::Text("null handle");
            return false;
        }
        //auto basetype = m_base_ctypename.str();
        auto childtype = m_obj->ctypename();
        ImGui::Text("shared %s (handle:%d)", childtype.c_str(), m_original_handle);
        return m_obj->imgui_widget(label, editable);
    }

#endif

    void on_cobject_event(const CObject& obj, ObjectEventType evt) override {
        post_cproperty_event(EPropertyEvent::data_edited);
    }
};

//------------------------------------------------------------------------------
// NODEREF
//------------------------------------------------------------------------------

// this is 0x2d-0xa long

class CNodeRefProperty : public CProperty {
protected:
    std::string m_str;

public:
    CNodeRefProperty(CPropertyOwner* owner)
        : CProperty(owner, EPropertyKind::NodeRef) {
    }

    ~CNodeRefProperty() override = default;

public:
    // overrides

    gname ctypename() const override {
        static gname sname("NodeRef");
        return sname;
    };

    bool serialize_in_impl(std::istream& is, CSystemSerCtx& serctx) override {
        uint16_t cnt = 0;
        is >> cbytes_ref(cnt);
        m_str.clear();
        m_str.resize(cnt, '\0');
        is.read(m_str.data(), cnt);
        return is.good();
    }

    virtual bool serialize_out(std::ostream& os, CSystemSerCtx& serctx) const {
        uint16_t cnt = (uint16_t)m_str.size();
        os << cbytes_ref(cnt);
        os.write(m_str.data(), cnt);
        return true;
    }

#ifndef DISABLE_CP_IMGUI_WIDGETS

    [[nodiscard]] bool imgui_widget_impl(const char* label, bool editable) override {
        ImGui::Text("noderef: %s", m_str.c_str());
        return false;
    }

#endif
};
