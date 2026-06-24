#pragma once
#include <string>
#include <set>
#include <vector>
#include <ostream>

/* ── Enum: type capabilities ── */
enum RetCap { RET_SERDE, RET_DISPLAY, RET_HANDLE, RET_UNKNOWN };
enum ArgCap { ARG_SERDE, ARG_FROMSTR, ARG_HANDLE, ARG_UNKNOWN };
struct TypeCap {
    RetCap ret;
    ArgCap arg;
};

/* ── Forward declarations (non-static, shared across split files) ── */

void parse_cfg_features(const std::string& attr, std::set<std::string>& out,
                        bool* had_filtered = nullptr);
RetCap return_strategy(const std::string& t);
ArgCap arg_strategy(const std::string& t);
bool is_non_serializable_type(const std::string& t);
bool check_skip_due_to_platform_cfg(const std::string& attr_body);
bool is_test_cfg(const std::string& attr_body);
std::string lookup_concrete_type(const std::string& pkg,
                                 const std::string& base_type);

bool parse_rust_fn(const std::string& content, size_t& offset,
                   std::string& name, std::string& args_str,
                   std::string& return_type,
                   bool& has_self, bool& is_async,
                   bool& has_generics,
                   std::set<std::string>* cfg_features = nullptr,
                   bool* had_filtered_features = nullptr,
                   bool* skip_platform = nullptr);

std::string rust_escape(const std::string& s);
int count_positional_args(const std::string& args_str);
std::string turbofish_type(const std::string& t);
bool is_result_type(const std::string& rt);
bool is_unsafe_feature(const std::string& feat);
std::vector<std::string> split_cfg_args(const std::string& s);
bool evaluate_simple_cfg(const std::string& s);
bool evaluate_cfg(const std::string& expr);
void gen_deser_args(std::string& s, const std::string& indent, int count,
                    const std::string& args_str = "",
                    bool has_self = false,
                    const std::string& pkg = "");
bool is_raw_ptr_type(const std::string& rt);
bool is_self_ref_type(const std::string& rt);
std::string placeholder_type(const std::string& rt);
bool is_never_type(const std::string& rt);
std::vector<std::string> extract_impl_type_params(const std::string& t);
bool method_references_generic_params(const std::string& args_str,
                                      const std::string& return_type,
                                      const std::vector<std::string>& params);
std::string fix_module_path(const std::string& type_name,
                            const std::string& pkg);
bool takes_self_by_value(const std::string& self_sig);
TypeCap type_capability(const std::string& base);
bool args_have_non_deserializable(const std::string& args_str);
