#pragma once
#include <inttypes.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <tools/rtti_dumper_dll/pe.hpp>

namespace dumper {

void dump();

// from https://github.com/yamashi/RED4ext.SDK

struct IRTTIType;
struct CClass;

struct CName
{
  constexpr CName(uint64_t aHash = 0) noexcept
    : hash(aHash)
  {}

  bool operator==(const CName& aRhs) const noexcept
  {
    return hash == aRhs.hash;
  }

  bool operator!=(const CName& aRhs) const noexcept
  {
    return hash != aRhs.hash;
  }

  operator bool() const noexcept
  {
    return hash != 0;
  }

  const char* ToString() const;

  uint64_t hash;
};

#pragma pack(push, 4)
struct CString
{
  char* c_str()
  {
    if (length >= 0x40000000u)
    {
        return text.ptr;
    }

    return text.str;
  }

  CString() = default;

private:

  union
  {
    char* ptr = nullptr;
    char str[0x14];

  } text = {};            // 00

  uint32_t length = 0;    // 14
  uint64_t capacity = 0;  // 18
};
#pragma pack(pop)

template<typename T>
struct DynArray
{
  //DynArray()
  //  : entries(nullptr)
  //  , size(0)
  //  , capacity(0)
  //{}

  const T& operator[](uint32_t aIndex) const
  {
    return entries[aIndex];
  }

  T& operator[](uint32_t aIndex)
  {
    return entries[aIndex];
  }

  const T* begin() const
  {
    return entries;
  }

  const T* end() const
  {
    return entries + size;
  }

  T* entries;        // 00
  uint32_t capacity; // 08
  uint32_t size;     // 0C

private:

  DynArray() = default;
};

template<typename K, typename T>
struct HashMap
{
  static const uint32_t INVALID_INDEX = -1;

  struct Node
  {
    uint32_t next;      // 00
    uint32_t hashedKey; // 04
    K key;              // 08
    T value;            // 10
  };

  struct NodeList
  {
    Node* GetNextAvailNode()
    {
      if (nextIdx == -1)
        return nullptr;

      if (size == nextIdx)
      {
        if (size + 1 < capacity)
        {
          ++size;
          return &nodes[nextIdx++];
        }
        else
        {
          Node* node = &nodes[nextIdx];
          nextIdx = -1;
          return node;
        }
      }
      else
      {
        Node* node = &nodes[nextIdx];
        nextIdx = node->next;
        return node;
      }
    }

    Node* nodes;       // 00
    uint32_t capacity; // 08
    uint32_t stride;   // 0C - sizeof(Node)
    uint32_t nextIdx;  // 10 - next available node index to write to
    uint32_t size;     // 14

  private:

    NodeList() = default;
  };

  void for_each(std::function<void(const K&, T&)> aFunctor) const
  {
    if (size == 0)
      return;

    for (uint32_t index = 0; index != capacity; ++index)
    {
      uint32_t idx = indexTable[index];
      while (idx != INVALID_INDEX)
      {
        Node* node = &nodeList.nodes[idx];
        aFunctor(node->key, node->value);
        idx = node->next;
      }
    }
  }

  uint32_t* indexTable; // 00
  uint32_t size;        // 08
  uint32_t capacity;    // 0C
  NodeList nodeList;    // 10
  uintptr_t allocator;  // 28

private:

  HashMap() = default;
};

struct CProperty
{
  struct Flags
  {
    uint64_t b0 : 1;           // 00
    uint64_t b1 : 1;           // 01
    uint64_t b2 : 1;           // 02
    uint64_t isNotSerialized : 1; // 03
    uint64_t isNotSerializedIfUnknownCond : 1; // 04
    uint64_t b5 : 1;           // 05
    uint64_t isReturn : 1;     // 06 - Is true when it is a return property, might be "isReturn".
    uint64_t b7 : 1;           // 07
    uint64_t b8 : 1;           // 08
    uint64_t isOut : 1;        // 09
    uint64_t isOptional : 1;   // 0A
    uint64_t b11 : 5;          // 0B
    uint64_t isPrivate : 1;    // 10
    uint64_t isProtected : 1;  // 11
    uint64_t isPublic : 1;     // 12
    uint64_t b19 : 2;          // 13
    uint64_t isScripted : 1;   // 15 - When true, acquire value from holder (isScripted?)
    uint64_t b22 : 5;          // 16
    uint64_t isHandle : 1;     // 1B
    uint64_t isPersistent : 1; // 1C
    uint64_t b29 : 34;
  };

  IRTTIType* type;      // 00
  CName name;           // 08
  CName group;          // 10
  CClass* parent;       // 18
  uint32_t valueOffset; // 20
  Flags flags;          // 28

  uintptr_t get_value_address(void* parent_instance);

private:

  CProperty() = default;
};

struct CClassFunction {};

enum class ERTTIType : uint8_t
{
  Name = 0,
  Fundamental = 1,
  Class = 2,
  Array = 3,
  Simple = 4,
  Enum = 5,
  StaticArray = 6,
  NativeArray = 7,
  Pointer = 8,
  Handle = 9,
  WeakHandle = 10,
  ResourceReference = 11,
  ResourceAsyncReference = 12,
  BitField = 13,
  LegacySingleChannelCurve = 14,
  ScriptReference = 15,
  FixedArray = 16
};

struct IRTTIType
{
  virtual ~IRTTIType() = 0; // 00

  virtual void GetName(CName& aOut) const = 0;                  // 08
  virtual uint32_t GetSize() const = 0;                         // 10
  virtual uint32_t GetAlignment() const = 0;                    // 18
  virtual ERTTIType GetType() const = 0;                        // 20
  virtual void GetTypeName(CString& aOut) const = 0;            // 28
  virtual void GetComputedName(CName& aOut) const = 0;          // 30
  virtual void Construct(void* aMemory) const = 0;              // 38
  virtual void Destroy(void* aMemory) const = 0;                // 40
  virtual bool IsEqual(const void* aLhs, const void* aRhs, uint64_t h = 0) = 0; // 48
  virtual void Assign(void* aLhs, const void* aRhs) = 0;        // 50
  virtual void Move(void* aLhs, const void* aRhs) = 0;          // 58
  virtual void sub_60() = 0;                                    // 60
  virtual bool ToString(const void* aInstance, CString& aOut) const = 0; // 68
  virtual bool sub_70() = 0;                                    // 70
  virtual bool sub_78() = 0;                                    // 78
  virtual void sub_80() = 0;                                    // 80
  virtual void sub_88() = 0;                                    // 88
  virtual bool Unk_90(uintptr_t unk1, uintptr_t unk2, CString& unk3, uintptr_t& unk4) = 0;                // 90
  virtual bool Unk_98(uintptr_t unk1, uintptr_t unk2, CString& unk3, uintptr_t& unk4, uint8_t unk5) = 0;  // 98
  virtual void sub_A0() = 0;                                    // A0
  virtual bool sub_A8() = 0;                                    // A8
  virtual void sub_B0() = 0;                                    // B0
  virtual void* GetAllocator() const = 0;                       // B8
};

struct ISerializable
{
    virtual IRTTIType* GetNativeType() = 0;       // 00
    virtual IRTTIType* GetParentType() = 0;       // 08
    virtual uint64_t GetAllocator() = 0; // 10
    virtual ~ISerializable() = 0;                 // 18
    virtual void sub_20() = 0;                    // 20
    virtual void sub_28() = 0;                    // 28
    virtual void sub_30() = 0;                    // 30
    virtual void sub_38() = 0;                    // 38
    virtual void sub_40() = 0;                    // 40
    virtual void sub_48() = 0;                    // 48
    virtual void sub_50() = 0;                    // 50
    virtual void sub_58() = 0;                    // 58
    virtual void sub_60() = 0;                    // 60
    virtual void sub_68() = 0;                    // 68
    virtual void sub_70() = 0;                    // 70
    virtual void sub_78() = 0;                    // 78
    virtual void sub_80() = 0;                    // 80
    virtual void sub_88() = 0;                    // 88
    virtual void sub_90() = 0;                    // 90
    virtual void sub_98() = 0;                    // 98
    virtual void sub_A0() = 0;                    // A0
    virtual void sub_A8() = 0;                    // A8
    virtual void sub_B0() = 0;                    // B0
    virtual void sub_B8() = 0;                    // B8
    virtual void sub_C0() = 0;                    // C0
    virtual void sub_C8() = 0;                    // C8
    virtual bool sub_D0() = 0;                    // D0
};

struct CRTTIType
  : IRTTIType
{
  int64_t unk8;
};

struct CArrayBase : CRTTIType
{
    virtual CRTTIType* GetInnerType() const = 0;                                                    // C0
    virtual bool sub_C8() = 0;                                                                      // C8 ret 1
    virtual uint32_t GetLength(void* aInstance) const = 0;                                 // D0
    virtual int32_t GetMaxLength() const = 0;                                                       // D8 ret -1
    virtual void* GetElement(void* aInstance, uint32_t aIndex) const = 0;         // E0
    virtual void* GetValuePointer(void* aInstance,
                                           uint32_t aIndex) const = 0;                              // E8 Same func at 0xE0 ?
    virtual int32_t sub_F0(void* aInstance, int32_t aIndex, void* aElement) = 0;  // F0
    virtual bool RemoveAt(void* aInstance, int32_t aIndex) = 0;                            // F8
    virtual bool InsertAt(void* aInstance, int32_t aIndex) = 0;                            // 100
    virtual bool Resize(void* aInstance, uint32_t aSize) = 0;                              // 108
};

struct CArray : CArrayBase
{
    CRTTIType* innerType; // 10
    CName name;           // 18
    CRTTIType* parent;    // 20
    uintptr_t unk28;      // 28
    uintptr_t unk30;      // 30
    uintptr_t unk38;      // 38
};

struct CStaticArray : CArrayBase
{
    CRTTIType* innerType; // 10
    int32_t size;         // 18
    uint32_t pad1C;       // 1C
    CName name;           // 20
    CName unk28;          // 28
};

struct CNativeArray : CArrayBase
{
    CRTTIType* innerType; // 10
    int32_t size;         // 18
    uint32_t pad1C;       // 1C
    CName name;           // 20
    CName unk28;          // 28
};

struct CBitfield
  : CRTTIType
{
  CName hash;         // 10
  CName unk18;        // 18
  uint8_t size;       // 20 - Size in bytes the instance will use
  uint8_t flags;      // 21
  uint16_t unk22;     // 22
  uint32_t unk24;     // 24
  uint64_t validBits; // 28
  CName bitNames[64]; // 30
};

struct CClass
  : CRTTIType
{
  virtual void sub_C0() = 0;
  virtual void sub_C8() = 0;
  virtual void sub_D0() = 0;
  virtual void InitCls(void* aMemory) = 0;
  virtual void DestroyCls(void* aMemory) = 0;
  virtual void sub_E8() = 0;

  struct Flags
  {
    uint32_t isAbstract : 1;   // 00
    uint32_t isNative : 1;     // 01
    uint32_t b2 : 1;           // 02
    uint32_t b3 : 1;           // 03
    uint32_t isStruct : 1;     // 04
    uint32_t b5 : 1;           // 05
    uint32_t isImportOnly : 1; // 06
    uint32_t isPrivate : 1;    // 07
    uint32_t isProtected : 1;  // 08
    uint32_t isTestOnly : 1;   // 09
    uint32_t b10 : 22;
  };

  bool IsA(const IRTTIType* aType) const;

  uintptr_t GetDefaultInstance() const;
  DynArray<CProperty*>* GetAllProps() const;

  CClass* parent;                           // 10
  CName name;                               // 18
  CName name2;                              // 20
  DynArray<CProperty*> props;               // 28
  DynArray<CProperty*> unk38;               // 38
  DynArray<CClassFunction*> funcs;          // 48
  DynArray<CClassFunction*> staticFuncs;    // 58
  uint32_t size;                            // 68 The size of the real class that can be constructed.
  int32_t holderSize;                       // 6C
  Flags flags;                              // 70
  uint32_t alignment;                       // 74
  int64_t unk78;                            // 78
  int64_t unk80;                            // 80
  int64_t unk88;                            // 88
  int64_t unk90;                            // 90
  int32_t unk98;                            // 98
  int32_t unk9C;                            // 9C
  int64_t unkA0;                            // A0
  HashMap<uint64_t, CClassFunction*> unkA8; // A8
  int64_t unkD8;                            // D8
  int64_t defaultInstance;                  // E0
  HashMap<uint64_t, CProperty*> unkE8;      // E8
  DynArray<CProperty*> unk118;              // 118 More entries than 0x28, will contain native props
  DynArray<void*> unk128;                   // 128
  DynArray<CProperty*> unk138;              // 138 Only RT_Class types?
  DynArray<void*> unk148;                   // 148
  DynArray<CProperty*> unk158;              // 158 Scripted props?
  DynArray<void*> unk168;                   // 168
  int64_t unk178;                           // 178
  DynArray<void*> unk180;                   // 180
  int8_t unk190[256];                       // 190
  int16_t unk290;                           // 290
  int32_t unk294;                           // 294
  int8_t unk298;                            // 298
  int8_t unk299;                            // 299
};

struct CEnum
  : CRTTIType
{
    CName hash;                   // 10
    CName unk18;                  // 18
    uint8_t size;                 // 20 - Size in bytes the instance will use
    uint8_t flags;                // 21
    uint16_t unk22;               // 22
    uint32_t unk24;               // 24
    DynArray<CName> hashList;     // 28
    DynArray<uint64_t> valueList; // 38
    DynArray<CName> unk48;        // 48
    DynArray<uint64_t> unk58;     // 58
};

struct CRTTISystem
{
  static CRTTISystem* Get();
  
  // 40 53 48 83 EC 20 65 48 8B 04 25 58 00 00 00 48 8D 1D

  virtual IRTTIType* GetType(CName aName) = 0;
  virtual void GetTypeByAsyncId(uint32_t aAsyncId) = 0;
  virtual CClass* GetClass(CName aName) = 0;
  virtual IRTTIType* GetEnum(CName aName) = 0;
  virtual IRTTIType* BitField(CName aName) = 0;

  uint64_t unk08;
  HashMap<CName, IRTTIType*> types;
};

} // namespace dumper

