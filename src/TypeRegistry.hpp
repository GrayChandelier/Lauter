#pragma once
#include <memory>
#include <vector>
#include "Details.hpp"

namespace Lauter
{
    struct BuiltinTypeCache
    {
        PrimitiveType* int8 = nullptr;
        PrimitiveType* int16 = nullptr;
        PrimitiveType* int32 = nullptr;
        PrimitiveType* real32 = nullptr;
        PrimitiveType* real64 = nullptr;
        PrimitiveType* boolType = nullptr;
        PrimitiveType* voidType = nullptr;
    };

    class TypeRegistry
    {
    private:
        std::vector<std::unique_ptr<SemanticType>> types;
    public:
        TypeRegistry() = default;
        template<typename T, typename... Args>
        T* create(Args&&... args)
        {
            auto type = std::make_unique<T>( std::forward<Args>(args)...);

            T* ptr = type.get();

            types.push_back(std::move(type));

            return ptr;
        }

        TypeRegistry(const TypeRegistry&) = delete;
        TypeRegistry& operator=(const TypeRegistry&) = delete;

    };
}