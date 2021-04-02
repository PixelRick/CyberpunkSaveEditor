#include <tools/rtti_dumper_dll/rtti.hpp>

#include <shlobj.h>
#include <filesystem>
#include <fstream>

namespace dumper {

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
  }
};

struct PropertyRecord
  : BaseRecord
{
  std::unique_ptr<TypeRecord> type;

  // expects the props array
  void dump(nlohmann::json& j) override
  {
    auto jp = nlohmann::json::object();
    jp["name"] = name.ToString();
    if (type)
    {
      type->dump(jp);
    }
    j.push_back(jp);
  }
};

struct ClassRecord
  : TypeRecord
{
  std::shared_ptr<ClassRecord> parent;

  std::vector<std::unique_ptr<PropertyRecord>> props;

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

    nlohmann::json jprops = nlohmann::json::array();

    for (auto& prop : props)
    {
      // i hope this is the true ordering
      prop->dump(jprops);
    }

    jc["props"] = jprops;

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

    nlohmann::json jmbrs = nlohmann::json::object();

    for (auto& member : members)
    {
      jmbrs[member.enumerator.ToString()] = member.value;
    }

    nlohmann::json jmbrs2 = nlohmann::json::object();

    for (auto& member : members2)
    {
      jmbrs2[member.enumerator.ToString()] = member.value;
    }

    auto sname = name.ToString();

    j[sname] = jmbrs;

    if (!jmbrs2.empty())
    {
      j[sname + std::string("_TEST")] = jmbrs2;
    }
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

  auto& props = cls->props;
  for (uint32_t i = 0; i < props.size; i++)
  {
    auto prop = new PropertyRecord();
    prop->name = props[i]->name;
    prop->etyp = (ERTTIType)0xFF;

    prop->type.reset(new TypeRecord());
    props[i]->type->GetName(prop->type->name);
    prop->type->etyp = props[i]->type->GetType();

    ptr->props.emplace_back(prop);
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

  {
    std::ofstream file(path / L"JClasses.json");
    file << jclasses.dump(2);
  }

  {
    std::ofstream file(path / L"JEnums.json");
    file << jenums.dump(2);
  }

  SPDLOG_INFO("dump finished");
}

} // namespace dumper

