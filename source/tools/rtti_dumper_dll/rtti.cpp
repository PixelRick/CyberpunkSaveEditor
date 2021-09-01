#include <tools/rtti_dumper_dll/rtti.hpp>

#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

std::string bytes_to_hex(const void* buf, size_t len)
{
  const uint8_t* const u8buf = (uint8_t*)buf;
  std::stringstream ss;
  ss << std::hex << std::uppercase;
  for (size_t i = 0; i < len; ++i)
    ss << std::setfill('0') << std::setw(2) << (uint32_t)u8buf[i];
  return ss.str();
}


#define GEN_OLD_DB_FORMAT 0

namespace dumper {

uintptr_t GetDataHolder(const void* a)
{
  using fn_t = uintptr_t (*)(const void*);

  static fn_t fn = []() -> fn_t {

    auto matches = find_pattern_in_game_text("\x40\x53\x48\x83\xEC\x20\x48\x83\x79\x38\x00\x48\x8B\xD9\x75\x3B", "xxxxxxxxxxxxxxxx");
    if (matches.size() != 1)
    {
      SPDLOG_ERROR("couldn't find GetDataHolder pattern");
      return (fn_t)nullptr;
    }

    return (fn_t)matches[0];

  }();

  if (fn)
  {
    return fn(a);
  }

  return 0;
}

uintptr_t CProperty::get_final_address(void* parent_instance)
{
  uintptr_t base = (uintptr_t)parent_instance;
  if (flags.isScripted)
  {
    base = GetDataHolder(parent_instance);
    if (base == 0)
    {
      return 0;
    }
  }
  return base + valueOffset;
}


void dump_prop_default_value(nlohmann::json::reference jref, IRTTIType* type, void* prop_addr, void* ref_prop_addr);

nlohmann::json dump_props_default_values(CClass* cls, void* instance, void* ref_instance = nullptr)
{
  auto j = nlohmann::json::object();

  auto pprops = cls->GetAllProps();
  if (!pprops)
  {
    j["!!!failed_to_get_props!!!"] = true;
    return j;
  }

  if (pprops->size == 0)
    return j;

  /*void* instance = nullptr;
  if (!cls->flags.isStruct)
  {
    if (top_instance)
    {
      instance = top_instance;
    }
    else
    {
      instance = (void*)cls->GetDefaultInstance();
    }
  }*/

  if (!instance)// && !cls->flags.isStruct)
  {
    instance = (void*)cls->GetDefaultInstance();
  }
  else if (!ref_instance)
  {
    ref_instance = (void*)cls->GetDefaultInstance();
  }

  if (!instance)
  {
    if (!cls->flags.isStruct)
      j["!!!failed_to_get_default_instance!!!"] = true;
    return j;
  }

  auto& props = *pprops;

  for (uint32_t i = 0; i < props.size; i++)
  {
    CProperty* pprop = props[i];
    if (!pprop || pprop->flags.isNotSerialized)
      continue;
    
    const auto prop_name = pprop->name.ToString();
    if (!prop_name)
    {
      j["!!!failed_to_get_prop_name!!!"] = true;
      continue;
    }

    uintptr_t prop_addr = pprop->get_final_address(instance);
    if (!prop_addr)
    {
      j[prop_name] = "!!!failed_to_get_addr!!!";
      continue;
    }

    uintptr_t ref_prop_addr = 0;
    if (ref_instance)
    {
      ref_prop_addr = pprop->get_final_address(ref_instance);
      if (!ref_prop_addr)
      {
        j[prop_name] = "!!!failed_to_get_ref_addr!!!";
        continue;
      }
    }

    auto obj = nlohmann::json::object();
    dump_prop_default_value(obj, pprop->type, (void*)prop_addr, (void*)ref_prop_addr);
    if (!obj.empty()) j[prop_name] = std::move(obj);
  }

  return j;
}

void dump_prop_default_value(nlohmann::json::reference jref, IRTTIType* type, void* prop_addr, void* ref_prop_addr)
{
  if (ref_prop_addr)
  {
    if (type->IsEqual(prop_addr, ref_prop_addr))
      return;
  }

  nlohmann::json j = nlohmann::json::object();

  static CString reused;

  switch (type->GetType())
  {
    case ERTTIType::Class:
    {
      auto diff = dump_props_default_values((CClass*)type, prop_addr, ref_prop_addr);
      if (!diff.empty())
        jref = diff; 
      break;
    }
    case ERTTIType::Name:
    {
      CName cn = *(CName*)prop_addr;
      const char* s = cn.ToString();
      if (s)
      {
        if (strcmp(s, "None"))
          jref = s;
      }
      else
        jref = cn.hash;

      break;
    }
    case ERTTIType::Fundamental:
    {
      CName cfundname;
      type->GetName(cfundname);
      std::string fundname = cfundname.ToString();
      if (fundname == "Bool")
      {
        auto val = (bool)(*(uint8_t*)prop_addr);
        if (val) jref = val;
      }
      else if (fundname == "Float")
      {
        auto val = *(float*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "Double")
      {
        auto val = *(double*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "Int8")
      {
        auto val = *(int8_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "Int16")
{
        auto val = *(int16_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "Int32")
      {
        auto val = *(int32_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "Int64")
      {
        auto val = *(int64_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "Uint8")
      {
        auto val = *(uint8_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "Uint16")
{
        auto val = *(uint16_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "Uint32")
      {
        auto val = *(uint32_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "Uint64")
      {
        auto val = *(uint64_t*)prop_addr;
        if (val) jref = val;
      }
      else
      {
        jref = "failed_to_get_fundamental";
      }
      break;
    }
    case ERTTIType::Simple:
    {
      CName cfundname;
      type->GetName(cfundname);
      std::string fundname = cfundname.ToString();
      if (fundname == "CDateTime")
      {
        auto val = *(uint64_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "CRUID")
      {
        auto val = *(uint64_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "CRUIDRef")
      {
        auto val = *(uint64_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "MessageResourcePath")
      {
        auto val = *(uint64_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "NodeRef")
      {
        auto val = *(uint64_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "RuntimeEntityRef")
      {
        auto val = *(uint64_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "SharedDataBuffer")
      {
        auto val = *(uint64_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "TweakDBID")
      {
        auto val = *(uint64_t*)prop_addr;
        if (val) jref = val;
      }
      else if (fundname == "CGUID")
      {
        auto pu64 = (uint64_t*)prop_addr;
        jref = {*pu64, *(pu64+1)};
      }
      else if (fundname == "EditorObjectID")
      {
        auto pu64 = (uint64_t*)prop_addr;
        jref = {*pu64, *(pu64+1), *(pu64+2), *(pu64+3)};
      }
      else if (fundname == "String")
      {
        auto val = ((CString*)prop_addr)->c_str();
        if (val && strlen(val) > 0) jref = val;
      }
      else if (fundname == "LocalizationString")
      {
        // always 0 by default it seems..
        CString cs = *(CString*)((char*)prop_addr + 8);
        auto lj = nlohmann::json::object();
        lj["LocalizationString.uk"] = *(uint64_t*)prop_addr;
        lj["LocalizationString.str"] = cs.c_str();
        if (*(uint64_t*)prop_addr != 0 || strlen(cs.c_str()) > 0)
          jref = lj;
      }
      else if (fundname == "Variant")
      {
        struct Variant
        {
          uint64_t type_bit;

          union{
            void* ptr;
            char buf[1];
          };

          IRTTIType* get_type()
          {
            return (IRTTIType*)(type_bit & ~0x7);
          }

          void* get_instance_ptr()
          {
            return (type_bit & 1) ? &buf[0] : ptr;
          }
        };

        auto var = (Variant*)prop_addr;
        auto ptr = var->get_instance_ptr();
        auto lj = nlohmann::json::object();
        if (ptr)
        {
          CName vtcn;
          var->get_type()->GetName(vtcn);
          auto vtn = vtcn.ToString();
          lj["Variant.type"] = strlen(vtn) ? vtn : fmt::format("<cn:{:016X}>", vtcn.hash);
          dump_prop_default_value(lj["Variant.instance_diff"], var->get_type(), (void*)ptr, nullptr);
          jref = lj;
        }
      }
      else
      {
        /*if (type->GetDebugString((void*)prop_addr, reused))
          jref = reused.c_str();
        else*/
          jref = "!!!failed_to_get_simple!!!";
      }

      break;

      //DataBuffer
      //EditorObjectID
      //gamedataLocKeyWrapper
      //multiChannelCurve:Float
      //serializationDeferredDataBuffer
    }
    case ERTTIType::Enum:
    {
      CEnum* ce = (CEnum*)type;
      uint64_t value = 0;
      memcpy((char*)&value, (void*)prop_addr, ce->GetSize());

      if (ref_prop_addr)
      {
        uint64_t ref_value = 0;
        memcpy((char*)&ref_value, (void*)ref_prop_addr, ce->GetSize());
        if (value == ref_value)
          break;
      }

      if (value)
      {
        bool found = false;
        for (uint32_t i = 0; i < ce->hashList.size; i++)
        {
          auto& enumerator = ce->hashList[i];
          if (ce->valueList[i] == value)
          {
            jref = ce->hashList[i].ToString();
            found = true;
            break;
          }
        }
        if (!found)
        {
          jref = value;
        }
      }
      break;
    }
    case ERTTIType::Array:
    {
      //CName cn; type->GetName(cn);
      // SPDLOG_CRITICAL("array type: {}", cn.ToString());
      auto lj = nlohmann::json::array();
      CArray* carr = (CArray*)type;
      IRTTIType* inner = carr->GetInnerType();
      for (uint32_t i = 0, n = carr->GetLength(prop_addr); i < n; ++i)
      {
        void* elt_addr = carr->GetValuePointer(prop_addr, i);
        auto p = nlohmann::json::object();
        dump_prop_default_value(p, inner, elt_addr, nullptr); // todo: use ref
        lj.push_back(p);
      }
      if (!lj.empty()) jref = lj;
      break;
    }
    case ERTTIType::StaticArray:
    {
      auto lj = nlohmann::json::array();
      CStaticArray* carr = (CStaticArray*)type;
      IRTTIType* inner = carr->GetInnerType();
      for (uint32_t i = 0, n = carr->GetLength(prop_addr); i < n; ++i)
      {
        void* elt_addr = carr->GetValuePointer(prop_addr, i);
        auto p = nlohmann::json::object();
        dump_prop_default_value(p, inner, elt_addr, nullptr); // todo: use ref
        lj.push_back(p);
      }
      if (!lj.empty()) jref = lj;
      break;
    }
    case ERTTIType::NativeArray:
    {
      auto lj = nlohmann::json::array();
      CNativeArray* carr = (CNativeArray*)type;
      IRTTIType* inner = carr->GetInnerType();
      for (uint32_t i = 0, n = carr->GetLength(prop_addr); i < n; ++i)
      {
        void* elt_addr = carr->GetValuePointer(prop_addr, i);
        auto p = nlohmann::json::object();
        dump_prop_default_value(p, inner, elt_addr, nullptr); // todo: use ref
        lj.push_back(p);
      }
      jref = lj;
      break;
    }
    case ERTTIType::BitField:
    {
      uint64_t value = 0;
      memcpy((char*)&value, (void*)prop_addr, type->GetSize());
      if (ref_prop_addr)
      {
        uint64_t ref_value = 0;
        memcpy((char*)&ref_value, (void*)ref_prop_addr, type->GetSize());
        if (value == ref_value)
          break;
      }
      if (value) jref = value;
      break;
    }
    case ERTTIType::Pointer:
    case ERTTIType::Handle:
    case ERTTIType::WeakHandle:
    {
      uint64_t ptr = *(uint64_t*)prop_addr;
      if (ptr)
      {
        struct HandleType : IRTTIType
        {
          uint64_t uk;
          IRTTIType* inner;
          CName name;
        };
        auto inner_type = ((HandleType*)type)->inner;
        if (inner_type->GetType() != ERTTIType::Class)
        {
          jref = "!!!unsupported_handle_type!!!";
        }
        else
        {
          auto subobj = dump_props_default_values((CClass*)inner_type, (void*)ptr, nullptr);
          CName inname;
          inner_type->GetName(inname);
          auto jh = nlohmann::json::object();
          jh["type"] = inname.ToString();
          if (subobj.empty())
          {
            jh["DEFAULT_INSTANCE"] = true;
          }
          else
          {
            jh["diff_vals"] = subobj;
          }
          jref = jh;
        }
      }
      break;
    }
    case ERTTIType::ResourceReference:
    {
      uint64_t ptr = *(uint64_t*)prop_addr;
      if (ptr)
      {
        jref = "!!!non_null_rref!!!";
      }
      break;
    }
    case ERTTIType::ResourceAsyncReference:
    {
      uint64_t ptr = *(uint64_t*)prop_addr;
      if (ptr)
      {
        //auto lj = nlohmann::json::object();
        /*auto cr = (ISerializable*)ptr;
        CName ptcn;
        cr->GetParentType()->GetName(ptcn);
        
        lj["res_type"] = ptcn.ToString();*/
        //lj["ptr"] = ptr;
        //lj["vtbl"] = (*(uint64_t*)ptr - (uint64_t)GetModuleHandle(0));
        jref = ptr;
      }
      break;
    }
    case ERTTIType::ScriptReference:
    {
      struct sref
      {
        uint64_t uk2[2];
        IRTTIType* inner;
        uint64_t ptr;
        CName name;
      };
      sref* p = (sref*)prop_addr;
      if (p->ptr || p->name.hash || p->inner)
      {
        std::string inner_name = "nullptr";
        if (p->inner)
        {
          CName innercn;
          p->inner->GetName(innercn);
          inner_name = innercn.ToString();
        }
        auto lj = nlohmann::json::object();
        lj["ScriptReference.inner"] = inner_name;
        lj["ScriptReference.ptr"] = p->ptr;
        lj["ScriptReference.name"] = p->name.ToString();
        jref = lj;
      }
      break;
    }
    case ERTTIType::LegacySingleChannelCurve:
    {
      struct CurveData {
        uint64_t unk00;
        void* data;
        uint32_t size;
        uint32_t capacity;
        uint64_t unk18;
        uint64_t unk20;
        uint64_t unk28;
        uint8_t unk30;
        uint8_t unk31;
      };
      auto cd = (CurveData*)prop_addr;
      auto lj = nlohmann::json::object();
      if (cd->data)
      {
        lj["data"] = bytes_to_hex(cd->data, cd->size);
      }
      lj["unk18"] = fmt::format("{:016X}", cd->unk18);
      lj["unk20"] = fmt::format("{:016X}", cd->unk20);
      lj["unk28"] = fmt::format("{:016X}", cd->unk28);
      lj["e30"] = fmt::format("{:02X}", cd->unk30);
      lj["e31"] = fmt::format("{:02X}", cd->unk31);
      jref = lj;
      break;
    }
    default:
    {
      jref = "!!!unknown ERTTIType!!!";
      break;
    }
  }
}


CRTTISystem* CRTTISystem::Get()
{
  using fn_t = CRTTISystem * (*)();

  static fn_t fn = []() -> fn_t {

    auto matches = find_pattern_in_game_text("\x40\x53\x48\x83\xEC\x20\x65\x48\x8B\x04\x25\x58\x00\x00\x00\x48\x8D\x1D", "xxxxxxxxxxxxxxxxxx");
    if (matches.size() != 1)
    {
      SPDLOG_ERROR("couldn't find CRTTISystem::Get pattern");
      return (fn_t)nullptr;
    }

    return (fn_t)matches[0];

  }();

  if (fn)
  {
    return fn();
  }

  return nullptr;
}

const char* CName::ToString() const
{
  using fn_t = const char* (*)(const CName&);

  static fn_t fn = []() -> fn_t {

    auto matches = find_pattern_in_game_text("\x48\x83\xEC\x38\x48\x8B\x11\x48\x8D\x4C\x24\x20\xE8", "xxxxxxxxxxxxx");
    if (matches.size() != 1)
    {
      SPDLOG_ERROR("couldn't find CNamePool::Get pattern");
      return (fn_t)nullptr;
    }

    return (fn_t)matches[0];

  }();

  if (fn)
  {
    return fn(*this);
  }

  return "<error>";
}

uintptr_t foo(uintptr_t (*fn)(const CClass*), const CClass* cls)
{
  __try
  {
      return fn(cls);
      //__debugbreak();
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
      SPDLOG_ERROR("exception in GetDefaultInstance with {}", cls->name.ToString());
  }
  return 0;
}

uintptr_t CClass::GetDefaultInstance() const
{
  using fn_t = uintptr_t (*)(const CClass*);

  static fn_t fn = []() -> fn_t {

    auto matches = find_pattern_in_game_text("\x40\x56\x48\x83\xEC\x30\x48\x8B\x81\xE0\x00\x00\x00\x48\x8B\xF1", "xxxxxxxxxxxxxxxx");
    if (matches.size() != 1)
    {
      SPDLOG_ERROR("couldn't find CClass::GetDefaultInstance pattern");
      return (fn_t)nullptr;
    }

    return (fn_t)matches[0];

  }();

  if (fn)
  {
    return foo(fn, this);
  }

  return 0;
}

DynArray<CProperty*>* CClass::GetAllProps() const
{
  using fn_t = DynArray<CProperty*>* (*)(const CClass*);

  static fn_t fn = []() -> fn_t {

    auto matches = find_pattern_in_game_text("\x40\x53\x48\x83\xEC\x20\x33\xD2\x48\x8B\xD9\xE8\x80\x33\x00\x00", "xxxxxxxxxxxxxxxx");
    if (matches.size() != 1)
    {
      SPDLOG_ERROR("couldn't find CClass::GetAllProps pattern");
      return (fn_t)nullptr;
    }

    return (fn_t)matches[0];

  }();

  if (fn)
  {
    return fn(this);
  }

  return 0;
}

// 

struct BaseRecord
{
  CName name;
  ERTTIType etyp = (ERTTIType)0xFF;

  virtual void dump(nlohmann::json& j) = 0;
};

using record_map_type = std::unordered_map<uint64_t, std::shared_ptr<BaseRecord>>;

record_map_type g_typerecs;


struct TypeRecord
  : BaseRecord
{
  void dump(nlohmann::json& j) override
  {
    j["ctypename"] = name.ToString();
#if !GEN_OLD_DB_FORMAT
    j["etyp"] = etyp;
#endif
  }
};

struct PropertyRecord
  : BaseRecord
{
  std::unique_ptr<TypeRecord> type;


  CName group;
  CProperty::Flags flags;

  // expects the props array
  void dump(nlohmann::json& j) override
  {
    auto jp = nlohmann::json::object();
    jp["name"] = name.ToString();
    if (type)
    {
      type->dump(jp);
    }
#if !GEN_OLD_DB_FORMAT
    jp["group"] = group.ToString();
    if (flags.isScripted)
    {
      jp["isScripted"] = true;
    }
    if (flags.isPrivate)
    {
      jp["isPrivate"] = true;
    }
    if (flags.isNotSerialized)
    {
      jp["isNotSerialized"] = true;
    }
    if (flags.isNotSerializedIfUnknownCond)
    {
      jp["isNotSerializedIfUnknownCond"] = true;
    }
#endif
    j.push_back(jp);
  }
};

struct ClassRecord
  : TypeRecord
{
  std::shared_ptr<ClassRecord> parent;

  std::vector<std::unique_ptr<PropertyRecord>> props;

  CClass::Flags flags;

  std::optional<nlohmann::json> default_values;

  // expects the root json
  void dump(nlohmann::json& j) override
  {
    auto jc = nlohmann::json::object();

    if (parent)
    {
      parent->dump(j); // dump dependencies first
      jc["parent"] = parent->name.ToString();
    }

    auto label = "";
    if (name.hash != 0)
      label = name.ToString();

    jc["ctypename"] = label;

#if !GEN_OLD_DB_FORMAT
    if (flags.isStruct) jc["isStruct"] = true;
    if (flags.isNative) jc["isNative"] = true;
    if (flags.isAbstract) jc["isAbstract"] = true;
    if (flags.isProtected) jc["isProtected"] = true;
    if (flags.isPrivate) jc["isPrivate"] = true;
#endif

    nlohmann::json jprops = nlohmann::json::array();

    for (auto& prop : props)
    {
      // i hope this is the true ordering
      prop->dump(jprops);
    }

    jc["props"] = jprops;

#if !GEN_OLD_DB_FORMAT
    if (default_values.has_value())
    {
      auto dv = default_values.value();
      if (!dv.empty())
        jc["default_values"] = dv;
    }
#endif

    j[label] = jc;
  }
};

struct EnumRecord
  : TypeRecord
{
  struct Member
  {
    CName enumerator;
    int64_t value;
  };

  size_t utsize;
  std::vector<Member> members;
  std::vector<Member> members2;

  // expects the root json
  void dump(nlohmann::json& j) override
  {
    //file << "enum " << name.ToString() << std::endl;

    std::sort(members.begin(), members.end(),
      [](auto& lhs, auto& rhs) { return lhs.value < rhs.value; }
    );

    std::sort(members2.begin(), members2.end(),
      [](auto& lhs, auto& rhs) { return lhs.value < rhs.value; }
    );

    nlohmann::json jmbrs = nlohmann::json::array();

#if GEN_OLD_DB_FORMAT
    for (auto& member : members)
    {
      jmbrs.push_back(member.enumerator.ToString());
    }
    auto sname = name.ToString();
    j[sname] = jmbrs;
#else
    for (auto& member : members)
    {
      //jmbrs[member.enumerator.ToString()] = member.value;
      nlohmann::json jmbr = nlohmann::json::object();
      jmbr["name"] = member.enumerator.ToString();
      jmbr["value"] = member.value;
      jmbrs.push_back(jmbr);
    }

    nlohmann::json jmbrs2 = nlohmann::json::array();

    for (auto& member : members2)
    {
      //jmbrs2[member.enumerator.ToString()] = member.value;
      nlohmann::json jmbr = nlohmann::json::object();
      jmbr["name"] = member.enumerator.ToString();
      jmbr["value"] = member.value;
      jmbrs2.push_back(jmbr);
    }

    auto sname = name.ToString();

    auto je = nlohmann::json::object();
    je["enumerators"] = jmbrs;
    je["compat_enumerators"] = jmbrs2;
    je["ut_size"] = utsize;
    j[sname] = je;
#endif
  }
};

std::pair<record_map_type::iterator, bool>
proc_class(CClass* cls)
{
  std::shared_ptr<ClassRecord> parent = nullptr;
  if (cls->parent)
  {
    auto processed = proc_class(cls->parent);
    parent = std::dynamic_pointer_cast<ClassRecord>(processed.first->second);
  }

  CName name;
  cls->GetName(name);

  auto it = g_typerecs.find(name.hash);
  if (it != g_typerecs.end())
  {
    return { it, false };
  }

  auto ptr = std::make_shared<ClassRecord>();
  ptr->name = name;
  ptr->parent = parent;
  ptr->etyp = cls->GetType();
  ptr->flags = cls->flags;

  auto& props = cls->props;
  for (uint32_t i = 0; i < props.size; i++)
  {
    auto proprec = new PropertyRecord();
    CProperty* pprop = props[i];
    proprec->name = pprop->name;
    proprec->group = pprop->group;
    proprec->etyp = (ERTTIType)0xFF;
    proprec->flags = pprop->flags;

    proprec->type.reset(new TypeRecord());
    pprop->type->GetName(proprec->type->name);
    proprec->type->etyp = pprop->type->GetType();

    ptr->props.emplace_back(proprec);
  }

  std::string cls_name = name.ToString();
  //if (cls_name == "appearanceAppearanceDefinition"
  //  || cls_name == "gameCommunitySystem"
  //  || cls_name == "vehicleAVBaseObject"
  //  || cls_name == "meshMeshAppearance")
  //{
  //  //SPDLOG_CRITICAL("{:016X}", (uint64_t)cls->GetAllocator());
  //}
  //else

  if (!cls->flags.isAbstract)
  {
    auto default_values = dump_props_default_values(cls, nullptr);
    if (!default_values.empty())
      ptr->default_values = default_values;
  }

  return g_typerecs.emplace(name.hash, ptr);
}

std::pair<record_map_type::iterator, bool>
proc_enum(CEnum* enm)
{
  CName name;
  enm->GetName(name);

  auto it = g_typerecs.find(name.hash);
  if (it != g_typerecs.end())
  {
    return { it, false };
  }

  auto ptr = std::make_shared<EnumRecord>();
  ptr->name = name;
  ptr->etyp = enm->GetType();
  ptr->utsize = enm->GetSize();

  for (uint32_t i = 0; i < enm->hashList.size; i++)
  {
    auto& enumerator = enm->hashList[i];
    auto& value = enm->valueList[i];

    EnumRecord::Member mbr;
    mbr.enumerator = enumerator;
    mbr.value = value;

    ptr->members.emplace_back(std::move(mbr));
  }

  for (uint32_t i = 0; i < enm->unk48.size; i++)
  {
    auto& enumerator = enm->unk48[i];
    auto& value = enm->unk58[i];

    EnumRecord::Member mbr;
    mbr.enumerator = enumerator;
    mbr.value = value;

    ptr->members2.emplace_back(std::move(mbr));
  }

  return g_typerecs.emplace(name.hash, ptr);
}


void dump()
{
  wchar_t* buf = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &buf)))
  {
    SPDLOG_ERROR("couldn't get path to the documents folder");
    return;
  }

  std::filesystem::path path = buf;
  CoTaskMemFree(buf);

  path /= "CPDumps";
  if (!std::filesystem::exists(path) && !std::filesystem::create_directories(path))
  {
    SPDLOG_ERROR("couldn't create CPDumps folder");
    return;
  }

  auto rtti = CRTTISystem::Get();
  SPDLOG_INFO("dump start");

  if (!rtti)
  {
    SPDLOG_ERROR("couldn't get the rtti system");
    return;
  }


  // --------------------------------

  struct something_ret
  {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
  };

  struct ArchiveAsyncSrc
  {
    virtual void fn00() = 0;
    virtual void fn08() = 0;
    virtual void fn10() = 0;
    virtual size_t get_seg0_csize() = 0;
    virtual size_t ret0() = 0;
    virtual size_t something() = 0;
  };

  //{
  //  std::ofstream file(path / L"test.txt");
  //  file << jclasses.dump(2);
  //}

  //return;

  // --------------------------------


  rtti->types.for_each(
    [](CName name, IRTTIType* typ)
    {
      typ->GetName(name);

      SPDLOG_INFO(name.ToString());

      if (typ->GetType() == ERTTIType::Class)
      {
        proc_class(static_cast<CClass*>(typ));
      }
      else if (typ->GetType() == ERTTIType::Enum)
      {
        proc_enum(static_cast<CEnum*>(typ));
      }
    }
  );

  nlohmann::json jenums = nlohmann::json::object();
  nlohmann::json jclasses = nlohmann::json::object();
  nlohmann::json jglob = nlohmann::json::object();

  for (auto& i : g_typerecs)
  {
    auto& ptr = i.second;
    if (std::dynamic_pointer_cast<ClassRecord>(ptr))
    {
      ptr->dump(jclasses);
    }
    else if (std::dynamic_pointer_cast<EnumRecord>(ptr))
    {
      ptr->dump(jenums);
    }
  }

#if GEN_OLD_DB_FORMAT
  {
    std::ofstream file(path / L"CObjectBPs.json");
    file << jclasses.dump(2);
  }

  {
    std::ofstream file(path / L"CEnums.json");
    file << jenums.dump(2);
  }
#else
  {
    std::ofstream file(path / L"JClasses_1.3.json");
    file << jclasses.dump(2);
  }

  {
    std::ofstream file(path / L"JEnums_1.3.json");
    file << jenums.dump(2);
  }
#endif

  SPDLOG_INFO("dump finished");
}

} // namespace dumper

