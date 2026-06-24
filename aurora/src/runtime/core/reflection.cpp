#include "runtime/reflection.hpp"

ReflectionRegistry& ReflectionRegistry::instance() {
    static ReflectionRegistry reg;
    return reg;
}

void ReflectionRegistry::register_type(const ReflectionType& type) {
    std::lock_guard<std::mutex> lock(mtx_);
    types_[type.name] = type;
}

const ReflectionType* ReflectionRegistry::get_type(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = types_.find(name);
    return (it != types_.end()) ? &it->second : nullptr;
}

std::vector<std::string> ReflectionRegistry::get_type_names() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> names;
    for (const auto& [name, _] : types_)
        names.push_back(name);
    return names;
}

void ReflectionRegistry::register_field(const std::string& type_name, const ReflectionField& field) {
    std::lock_guard<std::mutex> lock(mtx_);
    types_[type_name].fields.push_back(field);
}

void ReflectionRegistry::register_method(const std::string& type_name, const ReflectionMethod& method) {
    std::lock_guard<std::mutex> lock(mtx_);
    types_[type_name].methods.push_back(method);
}

std::vector<ReflectionField> ReflectionRegistry::get_fields(const std::string& type_name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = types_.find(type_name);
    return (it != types_.end()) ? it->second.fields : std::vector<ReflectionField>();
}

std::vector<ReflectionMethod> ReflectionRegistry::get_methods(const std::string& type_name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = types_.find(type_name);
    return (it != types_.end()) ? it->second.methods : std::vector<ReflectionMethod>();
}

extern "C" {

void aurora_reflection_register_type(const char* name, int64_t size) {
    if (!name) return;
    ReflectionType type;
    type.name = name;
    type.size = size;
    ReflectionRegistry::instance().register_type(type);
}

void aurora_reflection_register_field(const char* type_name, const char* field_name,
                                       const char* field_type, int64_t offset, int64_t size) {
    if (!type_name || !field_name) return;
    ReflectionField field;
    field.name = field_name;
    field.type_name = field_type ? field_type : "";
    field.offset = offset;
    field.size = size;
    ReflectionRegistry::instance().register_field(type_name, field);
}

void aurora_reflection_register_method(const char* type_name, const char* method_name,
                                        const char* return_type, void* fn_ptr) {
    if (!type_name || !method_name) return;
    ReflectionMethod method;
    method.name = method_name;
    method.return_type = return_type ? return_type : "void";
    method.fn_ptr = fn_ptr;
    ReflectionRegistry::instance().register_method(type_name, method);
}

}
