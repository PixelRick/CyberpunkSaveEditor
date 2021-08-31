#pragma once
#include <rttr/type>

namespace rttr::registration {

template<typename Type>
void alias(std::string_view name)
{
    auto t = type::get<Type>();
    detail::type_register::custom_name(t, name);
}

template<typename NativeType>
void native()
{
    type::get<NativeType>();
}

} // namespace rttr::registration

