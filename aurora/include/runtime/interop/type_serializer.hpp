#pragma once
#include "runtime/interop/type_ir.hpp"
#include <string>

struct TypeSerializer {
    static std::string to_json(const InteropType& type);
    static InteropType from_json(const std::string& json);

    static std::string to_json_registry(const InteropTypeRegistry& reg);
    static InteropTypeRegistry from_json_registry(const std::string& json);

    static std::string to_schema();
};
