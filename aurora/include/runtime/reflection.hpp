#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

struct ReflectionField {
    std::string name;
    std::string type_name;
    int64_t offset;
    int64_t size;
};

struct ReflectionMethod {
    std::string name;
    std::vector<std::string> param_types;
    std::string return_type;
    void* fn_ptr;
};

struct ReflectionType {
    std::string name;
    std::vector<ReflectionField> fields;
    std::vector<ReflectionMethod> methods;
    int64_t size;
};

class ReflectionRegistry {
public:
    static ReflectionRegistry& instance();

    void register_type(const ReflectionType& type);
    const ReflectionType* get_type(const std::string& name) const;
    std::vector<std::string> get_type_names() const;

    void register_field(const std::string& type_name, const ReflectionField& field);
    void register_method(const std::string& type_name, const ReflectionMethod& method);

    std::vector<ReflectionField> get_fields(const std::string& type_name) const;
    std::vector<ReflectionMethod> get_methods(const std::string& type_name) const;

private:
    ReflectionRegistry() = default;
    std::unordered_map<std::string, ReflectionType> types_;
    mutable std::mutex mtx_;
};

#ifdef __cplusplus
extern "C" {
#endif

void aurora_reflection_register_type(const char* name, int64_t size);
void aurora_reflection_register_field(const char* type_name, const char* field_name,
                                       const char* field_type, int64_t offset, int64_t size);
void aurora_reflection_register_method(const char* type_name, const char* method_name,
                                        const char* return_type, void* fn_ptr);

#ifdef __cplusplus
}
#endif
