#pragma once
#include <memory>

class CStringPool;
class CProperty;
class CObject;
class CSystem;
class CSystemSerCtx;

using CPropertyUPtr = std::unique_ptr<CProperty>;
using CObjectSPtr = std::shared_ptr<CObject>;

class CObjectBP;
class CFieldBP;

using CObjectBPSPtr = std::shared_ptr<CObjectBP>;

