#pragma once
#include <memory>

class CStringPool;
class CProperty;
class CObject;
class CSystem;
class CSystemSerCtx;

using CPropertySPtr = std::shared_ptr<CProperty>;
using CObjectSPtr = std::shared_ptr<CObject>;

class CObjectBP;
class CFieldDesc;

using CObjectBPSPtr = std::shared_ptr<CObjectBP>;

