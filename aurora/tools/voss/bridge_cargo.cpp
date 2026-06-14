#include "bridge_cargo_impl.hpp"
#include "bridge_shared.h"
#include <set>
CargoDiscovery discover_cargo_functions(const std::string& pkg,
                                         const std::string& ver,
                                         const std::string& dir)
{
    CargoDiscovery result; /* all fields default-empty, counts 0 */
    std::string tarball_path = fs::absolute(dir).string() + "/" + pkg + "-" + ver + ".crate";

    /* Download using system tool (avoids binary-data issues with http_get) */
    /* Try CDN URL first (works when crates.io API is down), fallback to API redirect */
    {
        std::vector<std::string> urls;
        urls.push_back("https://static.crates.io/crates/" + pkg + "/" + pkg + "-" + ver + ".crate");
        urls.push_back("https://crates.io/api/v1/crates/" + pkg + "/" + ver + "/download");

        bool downloaded = false;
        for (const auto& tarball_url : urls) {
            std::ostringstream dl_cmd;
#ifdef _WIN32
            dl_cmd << "powershell -NoProfile -Command \"Invoke-WebRequest -UserAgent 'Aurora-Voss/0.4 (github.com/anomalyco/aurora)' -Uri '"
                   << tarball_url << "' -OutFile '" << tarball_path << "'\" 2>$null";
#else
            dl_cmd << "curl -sSfL -H 'User-Agent: Aurora-Voss/0.4 (github.com/anomalyco/aurora)' -o \""
                   << tarball_path << "\" \"" << tarball_url << "\" 2>/dev/null";
#endif
            std::cout << "[bridge]   downloading crate source...\n";
            int dl_rc = std::system(dl_cmd.str().c_str());
            if (dl_rc == 0 && fs::exists(tarball_path) && fs::file_size(tarball_path) > 0) {
                downloaded = true;
                std::cout << "[bridge]   downloaded " << fs::file_size(tarball_path) << " bytes\n";
                break;
            }
        }
        if (!downloaded) {
            std::cerr << "[bridge] WARNING: crate download failed from all URLs\n";
            return result;
        }
    }

    /* Extract with tar */
    std::string extract_dir = (fs::temp_directory_path() / ("cargo_src_" + pkg)).string();
    fs::create_directories(extract_dir);
    {
        std::ostringstream tar_cmd;
#ifdef _WIN32
        tar_cmd << "tar xzf \"" << tarball_path << "\" -C \"" << extract_dir << "\" 2>NUL";
#else
        tar_cmd << "tar xzf \"" << tarball_path << "\" -C \"" << extract_dir << "\" 2>/dev/null";
#endif
        if (std::system(tar_cmd.str().c_str()) != 0) {
            std::cerr << "[bridge] WARNING: tar extraction failed\n";
            std::cerr << "[bridge]   path: " << tarball_path << "\n";
            return result;
        }
    }

    /* Check for proc-macro crate (no bridgeable functions) */
    {
        std::string cargo_toml_path = extract_dir + "/" + pkg + "-" + ver + "/Cargo.toml";
        std::ifstream cf(cargo_toml_path);
        if (cf.is_open()) {
            std::string ct((std::istreambuf_iterator<char>(cf)),
                            std::istreambuf_iterator<char>());
            if (ct.find("proc-macro = true") != std::string::npos) {
                std::cout << "[bridge]   (proc-macro crate — no bridgeable functions)\n";
                return result;
            }
        }
    }

    /* Read src/lib.rs + scan for mod declarations to include submodules */
    std::string pkg_prefix = pkg + "-" + ver;
    std::string src_dir = extract_dir + "/" + pkg_prefix + "/src";
    std::vector<std::string> source_files = { src_dir + "/lib.rs" };

    /* Read lib.rs to find declared modules */
    {
        std::ifstream lf(source_files[0]);
        if (lf.is_open()) {
            std::string lib_src((std::istreambuf_iterator<char>(lf)),
                                 std::istreambuf_iterator<char>());
            std::regex mod_re(R"((?:pub\s+)?mod\s+(\w+)\s*;)");
            auto mbegin = std::sregex_iterator(lib_src.begin(), lib_src.end(), mod_re);
            auto mend = std::sregex_iterator();
            for (auto it = mbegin; it != mend; ++it) {
                std::string mod_name = (*it)[1].str();
                /* Skip modules guarded by #[cfg(test)] */
                {
                    size_t pos = (size_t)(*it).position();
                    /* Scan backward past whitespace/newlines to find preceding attribute */
                    size_t attr_end = pos;
                    while (attr_end > 0 && (isspace((unsigned char)lib_src[attr_end - 1]) || lib_src[attr_end - 1] == ',')) attr_end--;
                    if (attr_end > 1 && lib_src[attr_end - 1] == ']') {
                        size_t attr_start = lib_src.rfind('[', attr_end - 1);
                        if (attr_start != std::string::npos && attr_start > 0 && lib_src[attr_start - 1] == '#') {
                            std::string attr_body = lib_src.substr(attr_start, attr_end - attr_start);
                            if (is_test_cfg(attr_body)) continue;
                        }
                    }
                }
                std::string mod_path1 = src_dir + "/" + mod_name + ".rs";
                std::string mod_path2 = src_dir + "/" + mod_name + "/mod.rs";
                if (fs::exists(mod_path1)) {
                    source_files.push_back(mod_path1);
                } else if (fs::exists(mod_path2)) {
                    source_files.push_back(mod_path2);
                }
            }
        }
    }

    /* Concatenate all source files for parsing */
    std::string content;
    for (const auto& src_file : source_files) {
        std::ifstream ifs(src_file);
        if (ifs.is_open()) {
            content += std::string((std::istreambuf_iterator<char>(ifs)),
                                    std::istreambuf_iterator<char>()) + "\n";
        }
    }

    if (content.empty()) {
        std::cerr << "[bridge] WARNING: no Rust source found in " << src_dir << "\n";
        return result;
    }

    /* ── Phase 1: collect all function signatures with impl context tracking ── */

    struct FnRec {
        std::string name;
        std::string args_str;
        std::string return_type;
        std::string impl_type;  /* empty = free fn */
        bool has_self;
        bool is_async;
        bool has_generics;
        int arg_count;
        bool is_result_ret;
    };
    std::vector<FnRec> fns;
    std::string current_impl;
    int impl_brace_depth = 0;
    std::set<std::string> all_features;

    {
        size_t off = 0;
        while (off < content.size()) {
            /* Detect impl TypeName { — skip attributes + extract cfg features */
            {
                size_t scan = off;
                while (scan < content.size()) {
                    if (isspace((unsigned char)content[scan])) { scan++; continue; }
                    if (scan + 1 < content.size() && content[scan] == '/' && content[scan+1] == '/') {
                        scan += 2; while (scan < content.size() && content[scan] != '\n') scan++;
                        continue;
                    }
                    if (scan + 1 < content.size() && content[scan] == '/' && content[scan+1] == '*') {
                        scan += 2; while (scan + 1 < content.size() && !(content[scan] == '*' && content[scan+1] == '/')) scan++;
                        scan += 2; continue;
                    }
                    /* Skip #[...] attributes and collect cfg features + detect cfg(test) */
                    if (scan < content.size() && content[scan] == '#') {
                        size_t attr_save = scan;
                        scan++;
                        if (scan < content.size() && content[scan] == '!') scan++;
                        if (scan < content.size() && content[scan] == '[') {
                            size_t bracket_start = scan + 1;
                            int ad = 1; scan++;
                            while (scan < content.size() && ad > 0) {
                                if (content[scan] == '[') ad++;
                                else if (content[scan] == ']') ad--;
                                scan++;
                            }
                            if (ad == 0) {
                                std::string attr_body = content.substr(bracket_start, scan - bracket_start - 1);
                                parse_cfg_features(attr_body, all_features);
                                /* Check for cfg(test) to skip test-only items */
                                if (is_test_cfg(attr_body)) {
                                    /* Skip the next item entirely */
                                    while (scan < content.size() && isspace((unsigned char)content[scan])) scan++;
                                    /* Skip keyword (fn, mod, struct, enum, impl, trait, etc.) */
                                    while (scan < content.size() && (isalnum((unsigned char)content[scan]) || content[scan] == '_')) scan++;
                                    /* Skip generics <...> */
                                    while (scan < content.size() && isspace((unsigned char)content[scan])) scan++;
                                    if (scan < content.size() && content[scan] == '<') {
                                        int ad2 = 1; scan++;
                                        while (scan < content.size() && ad2 > 0) {
                                            if (content[scan] == '<') ad2++;
                                            else if (content[scan] == '>') ad2--;
                                            scan++;
                                        }
                                    }
                                    /* Skip to first '{' or ';' (past item name, generics, where clause) */
                                    int paren_depth = 0, angle_depth = 0;
                                    bool in_str = false;
                                    while (scan < content.size()) {
                                        char cp = content[scan];
                                        if (in_str) {
                                            if (cp == '\\') { scan += 2; continue; }
                                            if (cp == '"') in_str = false;
                                            scan++; continue;
                                        }
                                        if (cp == '"') { in_str = true; scan++; continue; }
                                        if (cp == '(') { paren_depth++; scan++; continue; }
                                        if (cp == ')') { paren_depth--; scan++; continue; }
                                        if (cp == '<') { angle_depth++; scan++; continue; }
                                        if (cp == '>') { angle_depth--; scan++; continue; }
                                        if (cp == '/' && scan + 1 < content.size()) {
                                            if (content[scan+1] == '/') { scan += 2; while (scan < content.size() && content[scan] != '\n') scan++; continue; }
                                            if (content[scan+1] == '*') { scan += 2; while (scan + 1 < content.size() && !(content[scan] == '*' && content[scan+1] == '/')) scan++; scan += 2; continue; }
                                        }
                                        if (cp == '{' && paren_depth == 0 && angle_depth == 0) break;
                                        if (cp == ';' && paren_depth == 0 && angle_depth == 0) break;
                                        scan++;
                                    }
                                    /* Consume { ... } or ; */
                                    if (scan < content.size() && content[scan] == '{') {
                                        int body_depth = 1; scan++;
                                        while (scan < content.size() && body_depth > 0) {
                                            if (content[scan] == '"') { scan++; while (scan < content.size() && content[scan] != '"') { if (content[scan] == '\\') scan++; scan++; } if (scan < content.size()) scan++; continue; }
                                            if (content[scan] == '{') body_depth++;
                                            else if (content[scan] == '}') body_depth--;
                                            scan++;
                                        }
                                    } else if (scan < content.size() && content[scan] == ';') {
                                        scan++;
                                    }
                                    off = scan;
                                    goto next;
                                }
                                continue;
                            }
                        }
                        scan = attr_save;
                    }
                    break;
                }
                if (scan + 4 < content.size() && content.substr(scan, 4) == "impl" &&
                    (scan + 4 >= content.size() || !isalnum((unsigned char)content[scan+4]))) {
                    size_t imp = scan + 4;
                    while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                    /* ── Skip generic parameters: e.g. impl<T: Display, U> Type ── */
                    if (imp < content.size() && content[imp] == '<') {
                        int angle_depth = 1;
                        imp++;
                        while (imp < content.size() && angle_depth > 0) {
                            if (content[imp] == '<') angle_depth++;
                            else if (content[imp] == '>') angle_depth--;
                            imp++;
                        }
                    }
                    while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                    /* Skip the trait path (including its generics) and check for `for` */
                    size_t trait_path_end = imp;
                    /* Scan to end of trait path: stop at whitespace not inside angle brackets */
                    {
                        int ad = 0;
                        while (trait_path_end < content.size()) {
                            char c = content[trait_path_end];
                            if (c == '<') ad++;
                            else if (c == '>') ad--;
                            else if (ad == 0 && isspace((unsigned char)c)) break;
                            else if (ad == 0 && c == '{') break;
                            trait_path_end++;
                        }
                    }
                    size_t sp = trait_path_end;
                    while (sp < content.size() && isspace((unsigned char)content[sp])) sp++;
                    if (sp + 3 < content.size() && content.substr(sp, 4) == "for ") {
                        imp = sp + 4;
                        while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                    }
                    size_t type_start = imp;
                    while (imp < content.size() &&
                           (isalnum((unsigned char)content[imp]) || content[imp] == '_' ||
                            content[imp] == ':' || content[imp] == '!')) {
                        imp++;
                    }
                    if (imp > type_start) {
                        std::string candidate = content.substr(type_start, imp - type_start);
                        while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                        if (candidate == "DrainFilter") {
                            std::cerr << "[dbg] found DrainFilter at imp=" << imp << " char='" << content[imp] << "'\n";
                            std::cerr << "[dbg] context: " << content.substr(imp, 60) << "\n";
                        }
                        /* Capture generic args: e.g. impl HashMap<K, V> { ... } */
                        if (imp < content.size() && content[imp] == '<') {
                            size_t gstart = imp;
                            int ad2 = 1; imp++;
                            while (imp < content.size() && ad2 > 0) {
                                if (content[imp] == '<') ad2++;
                                else if (content[imp] == '>') ad2--;
                                imp++;
                            }
                            candidate += content.substr(gstart, imp - gstart);
                            while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                        }
                        /* Skip where clause: impl<A: Array> SmallVec<A> where A::Item: Copy { ... } */
                        if (imp + 5 < content.size() && content.substr(imp, 5) == "where") {
                            int paren_depth = 0, angle_depth = 0;
                            imp += 5;
                            while (imp < content.size()) {
                                if (content[imp] == '(') paren_depth++;
                                else if (content[imp] == ')') paren_depth--;
                                else if (content[imp] == '<') angle_depth++;
                                else if (content[imp] == '>') {
                                    /* Distinguish -> (fat arrow) from > (closing bracket) */
                                    if (angle_depth == 0) { imp++; continue; }  /* skip stray > */
                                    angle_depth--;
                                } else if (content[imp] == '{' && paren_depth == 0 && angle_depth == 0) break;
                                else if (content[imp] == '/' && imp + 1 < content.size()) {
                                    if (content[imp+1] == '/') {
                                        imp += 2; while (imp < content.size() && content[imp] != '\n') imp++;
                                        continue;
                                    }
                                    if (content[imp+1] == '*') {
                                        imp += 2;
                                        while (imp + 1 < content.size() && !(content[imp] == '*' && content[imp+1] == '/')) imp++;
                                        imp += 2; continue;
                                    }
                                }
                                imp++;
                            }
                            while (imp < content.size() && isspace((unsigned char)content[imp])) imp++;
                        }
                        if (imp < content.size() && content[imp] == '{') {
                            current_impl = candidate;
                            impl_brace_depth = 1;
                            off = imp + 1;
                            continue;
                        }
                    }
                }
            }

            /* Track brace depth inside impl — handle immediate } only */
            if (!current_impl.empty()) {
                size_t i = off;
                while (i < content.size() && isspace((unsigned char)content[i])) i++;
                if (i < content.size() && content[i] == '}') {
                    impl_brace_depth--;
                    if (impl_brace_depth == 0) {
                        current_impl.clear();
                    }
                    off = i + 1;
                    continue;
                }
                if (i < content.size() && content[i] == '{') {
                    impl_brace_depth++;
                    off = i + 1;
                    continue;
                }
            }

            /* Parse next function */
            {
                FnRec rec;
                rec.impl_type = current_impl;
                std::set<std::string> fn_features;
                bool had_filtered = false, skip_plat = false;
                if (!parse_rust_fn(content, off, rec.name, rec.args_str, rec.return_type,
                                    rec.has_self, rec.is_async, rec.has_generics, &fn_features, &had_filtered, &skip_plat)) {
                    off++; continue;
                }
                if (rec.name == "shave_the_yak") {
                    rec.name = "";
                }
                /* Advance past where clause and function body { ... } or ; */
                {
                    size_t p = off;
                    while (p < content.size() && isspace((unsigned char)content[p])) p++;
                    /* Skip where clause */
                    if (p + 5 < content.size() && content.substr(p, 5) == "where") {
                        int where_depth = 1;
                        p += 5;
                        while (p < content.size() && where_depth > 0) {
                            if (content[p] == '(') where_depth++;
                            else if (content[p] == ')') where_depth--;
                            else if (content[p] == '{' || content[p] == ';') break;
                            else if (content[p] == '/' && p + 1 < content.size()) {
                                if (content[p+1] == '/') {
                                    p += 2; while (p < content.size() && content[p] != '\n') p++;
                                    continue;
                                }
                                if (content[p+1] == '*') {
                                    p += 2; while (p + 1 < content.size() && !(content[p] == '*' && content[p+1] == '/')) p++;
                                    p += 2; continue;
                                }
                            }
                            p++;
                        }
                    }
                    while (p < content.size() && isspace((unsigned char)content[p])) p++;
                    /* Consume function body { ... } or ; */
                    if (p < content.size() && content[p] == '{') {
                        bool inside_impl = !current_impl.empty();
                        if (inside_impl) impl_brace_depth++;
                        int body_depth = 1;
                        p++;
                        bool in_string = false;
                        while (p < content.size() && body_depth > 0) {
                            if (in_string) {
                                if (content[p] == '\\') { p += 2; continue; }
                                if (content[p] == '"') in_string = false;
                                p++; continue;
                            }
                            if (content[p] == '"') { in_string = true; p++; continue; }
                            if (content[p] == '{') body_depth++;
                            else if (content[p] == '}') body_depth--;
                            else if (content[p] == '/' && p + 1 < content.size()) {
                                if (content[p+1] == '/') {
                                    p += 2; while (p < content.size() && content[p] != '\n') p++;
                                    continue;
                                }
                                if (content[p+1] == '*') {
                                    p += 2; while (p + 1 < content.size() && !(content[p] == '*' && content[p+1] == '/')) p++;
                                    p += 2; continue;
                                }
                            }
                            p++;
                        }
                        off = (p < content.size()) ? p + 1 : p;
                        if (inside_impl) impl_brace_depth--;
                    } else if (p < content.size() && content[p] == ';') {
                        off = p + 1;
                    }
                }
                /* Skip due to cfg filtering (platform, test, etc.) */
                if (skip_plat) {
                    std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                    std::cout << "[bridge]     skip " << ctx << " (cfg filtered)\n";
                    continue;
                }
                if (rec.has_generics) {
                    if (!rec.impl_type.empty())
                        std::cout << "[bridge]     skip " << rec.impl_type << "::" << rec.name << " (generic)\n";
                    else
                        std::cout << "[bridge]     skip " << rec.name << " (generic)\n";
                    continue;
                }
                /* If function had features but all were filtered (e.g. unstable/nightly), skip it */
                if (had_filtered && fn_features.empty()) {
                    std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                    std::cout << "[bridge]     skip " << ctx << " (requires nightly/unstable)\n";
                    continue;
                }
                /* Skip never type return (can't serialize) */
                if (is_never_type(rec.return_type)) {
                    std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                    std::cout << "[bridge]     skip " << ctx << " (returns never type)\n";
                    continue;
                }
                /* Check if return type is handle-based (opaque) — handle per context */
                {
                    RetCap rt = return_strategy(rec.return_type);
                    if (rt == RET_HANDLE && rec.impl_type.empty()) {
                        /* Free fn returning opaque → skip (useless without type context) */
                        std::string ctx = rec.name;
                        std::cout << "[bridge]     skip " << ctx << " (opaque return, free fn)\n";
                        continue;
                    }
                    if (rt == RET_HANDLE) {
                        std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                        std::cout << "[bridge]     gen  " << ctx << " (opaque return → handle)\n";
                    } else if (rt == RET_DISPLAY) {
                        std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                        std::cout << "[bridge]     gen  " << ctx << " (Display return → string)\n";
                    }
                }
                /* Check if any argument type is handle-based — only skip if truly opaque */
                if (args_have_non_deserializable(rec.args_str)) {
                    /* Check if args use FromStr — those are fine */
                    std::string astr2 = rec.args_str;
                    bool has_opaque_arg = false;
                    size_t pos2 = 0;
                    while (pos2 < astr2.size()) {
                        size_t colon = astr2.find(':', pos2);
                        if (colon == std::string::npos) break;
                        size_t tstart = colon + 1;
                        while (tstart < astr2.size() && isspace((unsigned char)astr2[tstart])) tstart++;
                        /* Extract type name */
                        size_t ti = tstart;
                        while (ti < astr2.size() && (astr2[ti] == '&' || isspace((unsigned char)astr2[ti]))) ti++;
                        while (ti < astr2.size() && astr2[ti] == '\'') { ti++; while (ti < astr2.size() && (isalnum((unsigned char)astr2[ti]) || astr2[ti] == '_')) ti++; }
                        while (ti < astr2.size() && isspace((unsigned char)astr2[ti])) ti++;
                        if (ti + 3 <= astr2.size() && astr2.substr(ti, 3) == "mut") { ti += 3; while (ti < astr2.size() && isspace((unsigned char)astr2[ti])) ti++; }
                        if (ti + 5 <= astr2.size() && astr2.substr(ti, 5) == "const") { ti += 5; while (ti < astr2.size() && isspace((unsigned char)astr2[ti])) ti++; }
                        std::string base_t;
                        int ad = 0;
                        while (ti < astr2.size() && (ad > 0 || (!isspace((unsigned char)astr2[ti]) && astr2[ti] != ',' && astr2[ti] != ')'))) {
                            if (astr2[ti] == '<') ad++;
                            if (astr2[ti] == '>') ad--;
                            base_t += astr2[ti]; ti++;
                        }
                        ArgCap cap = arg_strategy(base_t);
                        if (cap == ARG_HANDLE || cap == ARG_UNKNOWN) {
                            has_opaque_arg = true; break;
                        }
                        if (cap == ARG_FROMSTR) {
                            std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                            std::cout << "[bridge]     gen  " << ctx << " (FromStr arg)\n";
                        }
                        /* Move to next arg */
                        int ad2 = 0, pd = 0, bd = 0;
                        bool ins = false;
                        while (tstart < astr2.size()) {
                            char cp = astr2[tstart];
                            if (ins) { if (cp == '\\') tstart += 2; else { if (cp == '"') ins = false; tstart++; } continue; }
                            if (cp == '"') { ins = true; tstart++; continue; }
                            if (cp == '(') pd++;
                            if (cp == ')') pd--;
                            if (cp == '<') ad2++;
                            if (cp == '>') ad2--;
                            if (cp == '[') bd++;
                            if (cp == ']') bd--;
                            if (pd == 0 && ad2 == 0 && bd == 0 && (cp == ',' || cp == ')')) break;
                            tstart++;
                        }
                        pos2 = tstart + 1;
                    }
                    if (has_opaque_arg) {
                        std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                        std::cout << "[bridge]     skip " << ctx << " (opaque/handle args)\n";
                        continue;
                    }
                }
                /* Skip if arg type is Self (invalid outside impl) */
                if (rec.args_str.find("Self") != std::string::npos) {
                    std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                    std::cout << "[bridge]     skip " << ctx << " (Self arg type)\n";
                    continue;
                }
                /* Skip if arg type references a generic associated type (e.g. A::Item) */
                {
                    std::string astr = rec.args_str;
                    bool has_generic_arg = false;
                    /* Check for single-uppercase-letter followed by :: anywhere in args */
                    for (size_t ui = 1; ui + 1 < astr.size(); ui++) {
                        if (isupper((unsigned char)astr[ui-1]) &&
                            astr[ui] == ':' && astr[ui+1] == ':') {
                            if (ui < 2 || astr[ui-2] != ':') {
                                has_generic_arg = true; break;
                            }
                        }
                    }
                    /* Also check each argument type for single uppercase letter (existing check) */
                    if (!has_generic_arg) {
                        size_t ap = 0;
                        while (ap < astr.size()) {
                            size_t colon = astr.find(':', ap);
                            if (colon == std::string::npos) break;
                            size_t tstart = colon + 1;
                            while (tstart < astr.size() && isspace((unsigned char)astr[tstart])) tstart++;
                            if (tstart >= astr.size()) break;
                            size_t type_start = tstart;
                            if (astr[type_start] == '&') type_start++;
                            while (type_start < astr.size() && isspace((unsigned char)astr[type_start])) type_start++;
                            std::string tname;
                            size_t te = type_start;
                            while (te < astr.size() && !isspace((unsigned char)astr[te]) &&
                                   astr[te] != ',' && astr[te] != ')' && astr[te] != '<' &&
                                   astr[te] != '(' && astr[te] != '[' && astr[te] != '>' &&
                                   astr[te] != ']') {
                                if (astr[te] == '\'') {
                                    te++; while (te < astr.size() && (isalnum((unsigned char)astr[te]) || astr[te] == '_')) te++;
                                    continue;
                                }
                                if (te + 1 < astr.size() && astr[te] == ':' && astr[te+1] == ':') {
                                    tname.clear(); te += 2; continue;
                                }
                                tname += astr[te]; te++;
                            }
                            if (!tname.empty() && tname.size() == 1 && isupper((unsigned char)tname[0])) {
                                has_generic_arg = true; break;
                            }
                            int paren_depth = 0, angle_depth = 0, bracket_depth = 0;
                            bool in_str = false;
                            size_t aend = tstart;
                            while (aend < astr.size()) {
                                char c = astr[aend];
                                if (in_str) {
                                    if (c == '\\') { aend += 2; continue; }
                                    if (c == '"') in_str = false;
                                    aend++; continue;
                                }
                                if (c == '"') { in_str = true; aend++; continue; }
                                if (c == '(') { paren_depth++; aend++; continue; }
                                if (c == ')') { paren_depth--; aend++; continue; }
                                if (c == '<') { angle_depth++; aend++; continue; }
                                if (c == '>') { angle_depth--; aend++; continue; }
                                if (c == '[') { bracket_depth++; aend++; continue; }
                                if (c == ']') { bracket_depth--; aend++; continue; }
                                if (paren_depth == 0 && angle_depth == 0 && bracket_depth == 0 && (c == ',' || c == ')')) break;
                                aend++;
                            }
                            ap = aend + 1;
                        }
                    }
                    if (has_generic_arg) {
                        std::string ctx = rec.impl_type.empty() ? rec.name : rec.impl_type + "::" + rec.name;
                        std::cout << "[bridge]     skip " << ctx << " (generic param in args)\n";
                        continue;
                    }
                }
                /* Skip known non-existent public functions (parser artifacts) */
                if (rec.impl_type.empty()) {
                    if (rec.name == "new_random" || rec.name == "fill" ||
                        rec.name == "is_leader" || rec.name == "force" ||
                        rec.name == "shave_the_yak" ||
                        /* itertools parser artifacts */
                        rec.name == "minmax" || rec.name == "size_hint" ||
                        rec.name == "with_value" || rec.name == "into_parts" ||
                        rec.name == "sum" || rec.name == "product" ||
                        rec.name == "len" || rec.name == "count" ||
                        rec.name == "get_next" || rec.name == "prefill" ||
                        rec.name == "get_at" || rec.name == "reset_peek" ||
                        rec.name == "peek" || rec.name == "peek_mut" ||
                        rec.name == "peek_nth_mut" || rec.name == "next_if" ||
                        rec.name == "add" || rec.name == "add_scalar" ||
                        rec.name == "sub_scalar" || rec.name == "mul" ||
                        rec.name == "mul_scalar" || rec.name == "into_buffer" ||
                        rec.name == "max" || rec.name == "min" ||
                        rec.name == "put_back" || rec.name == "peek_nth" ||
                        rec.name == "output" || rec.name == "bits_mut" ||
                         rec.name == "force_mut" || rec.name == "hamming" ||
                         /* tokio parser artifacts */
                         rec.name == "get_kill_on_drop" || rec.name == "start_kill" ||
                         rec.name == "kill" || rec.name == "wait" ||
                         rec.name == "try_wait" || rec.name == "wait_with_output" ||
                         /* parking_lot parser artifacts */
                         rec.name == "to_deadline") {
                        std::string ctx = rec.name;
                        std::cout << "[bridge]     skip " << ctx << " (not a public free function)\n";
                        continue;
                    }
                }
                /* Skip known non-existent types (parser artifacts or feature-gated) */
                if (!rec.impl_type.empty()) {
                    std::string check_type = rec.impl_type;
                    size_t g = check_type.find('<');
                    if (g != std::string::npos) check_type = check_type.substr(0, g);
                    size_t c = check_type.rfind("::");
                    if (c != std::string::npos) check_type = check_type.substr(c + 2);
                    if (check_type == "AtomicStatus") {
                        std::cout << "[bridge]     skip " << check_type << "::" << rec.name << " (not found in crate)\n";
                        continue;
                    }
                }
                rec.arg_count = count_positional_args(rec.args_str);
                if (rec.has_self) rec.arg_count--;
                rec.is_result_ret = is_result_type(rec.return_type);
                /* Collect cfg features */
                for (const auto& f : fn_features)
                    all_features.insert(f);
                fns.push_back(rec);
            }
        next:
            continue;
        }
    }

    /* Validate features against Cargo.toml [features] section */
    {
        std::string cargo_toml_path = extract_dir + "/" + pkg_prefix + "/Cargo.toml";
        std::ifstream cf(cargo_toml_path);
        std::set<std::string> valid_features;
        if (cf.is_open()) {
            std::string line;
            bool in_features = false;
            while (std::getline(cf, line)) {
                /* Trim */
                size_t s = 0; while (s < line.size() && isspace((unsigned char)line[s])) s++;
                size_t e = line.size(); while (e > s && isspace((unsigned char)line[e-1])) e--;
                std::string trimmed = line.substr(s, e - s);
                if (trimmed.empty()) continue;
                if (trimmed.rfind("[", 0) == 0) {
                    /* Section header — stop at [dependencies] or [target.*] or [lib] */
                    in_features = (trimmed == "[features]");
                    continue;
                }
                if (in_features) {
                    size_t eq = trimmed.find('=');
                    if (eq != std::string::npos) {
                        std::string feat_name = trimmed.substr(0, eq);
                        size_t fs = 0; while (fs < feat_name.size() && isspace((unsigned char)feat_name[fs])) fs++;
                        size_t fe = feat_name.size(); while (fe > fs && isspace((unsigned char)feat_name[fe-1])) fe--;
                        if (fe > fs) valid_features.insert(feat_name.substr(fs, fe - fs));
                    }
                }
            }
        }
        /* Filter: only keep features that exist in Cargo.toml */
        if (!valid_features.empty()) {
            std::set<std::string> filtered;
            for (const auto& f : all_features) {
                if (valid_features.count(f))
                    filtered.insert(f);
                else
                    std::cout << "[bridge]     skip feature \"" << f << "\" (not in crate's Cargo.toml)\n";
            }
            all_features.swap(filtered);
        }
    }

    /* ── Filter out mutually exclusive feature groups ── */
    /* Known groups: max_level_* (log crate) — these conflict when combined */
    {
        /* Collect features into groups by prefix */
        std::set<std::string> keep;
        /* max_level_ group: keep only the highest one */
        std::string max_level_kept;
        static const char* max_level_order[] = {
            "max_level_off", "max_level_error", "max_level_warn",
            "max_level_info", "max_level_debug", "max_level_trace"
        };
        for (const auto& f : all_features) {
            if (f.rfind("max_level_", 0) == 0) {
                if (max_level_kept.empty()) {
                    max_level_kept = f;
                    keep.insert(f);
                } else {
                    /* Keep the higher level (later in the order array) */
                    int old_rank = -1, new_rank = -1;
                    for (int ri = 0; ri < 6; ri++) {
                        if (max_level_kept == max_level_order[ri]) old_rank = ri;
                        if (f == max_level_order[ri]) new_rank = ri;
                    }
                    if (new_rank > old_rank) {
                        keep.erase(max_level_kept);
                        keep.insert(f);
                        max_level_kept = f;
                    }
                    std::cout << "[bridge]     skip redundant feature \"" << f
                              << "\" (mutually exclusive with \"" << max_level_kept << "\")\n";
                }
            } else {
                keep.insert(f);
            }
        }
        all_features.swap(keep);
    }
    /* release_max_level_ group: keep only the highest one */
    {
        std::set<std::string> keep;
        std::string release_max_level_kept;
        static const char* release_max_level_order[] = {
            "release_max_level_off", "release_max_level_error", "release_max_level_warn",
            "release_max_level_info", "release_max_level_debug", "release_max_level_trace"
        };
        for (const auto& f : all_features) {
            if (f.rfind("release_max_level_", 0) == 0) {
                if (release_max_level_kept.empty()) {
                    release_max_level_kept = f;
                    keep.insert(f);
                } else {
                    int old_rank = -1, new_rank = -1;
                    for (int ri = 0; ri < 6; ri++) {
                        if (release_max_level_kept == release_max_level_order[ri]) old_rank = ri;
                        if (f == release_max_level_order[ri]) new_rank = ri;
                    }
                    if (new_rank > old_rank) {
                        keep.erase(release_max_level_kept);
                        keep.insert(f);
                        release_max_level_kept = f;
                    }
                    std::cout << "[bridge]     skip redundant feature \"" << f
                              << "\" (mutually exclusive with \"" << release_max_level_kept << "\")\n";
                }
            } else {
                keep.insert(f);
            }
        }
        all_features.swap(keep);
    }
    /* parking_lot: send_guard and deadlock_detection cannot be used together.
       Keep send_guard (more generally useful), drop deadlock_detection. */
    if (all_features.count("send_guard") && all_features.count("deadlock_detection")) {
        std::cout << "[bridge]     skip feature \"deadlock_detection\""
                  << " (mutually exclusive with \"send_guard\")\n";
        all_features.erase("deadlock_detection");
    }

    /* Copy cfg features to result */
    for (const auto& f : all_features)
        result.required_features.push_back(f);

    /* ── Phase 2: classify and generate code ── */

    /* Group by impl type for methods/constructors */
    struct TypeBucket {
        std::string crate_type;   /* e.g. "Mutex" */
        std::string safe_type;    /* e.g. "Mutex" */
        std::string subst_type;   /* e.g. "Mutex<()>" if generic, empty otherwise */
        std::vector<FnRec> methods;
        std::vector<FnRec> ctors;
    };
    std::map<std::string, TypeBucket> types;

    for (const auto& rec : fns) {
        if (rec.impl_type.empty()) {
            /* Free function — generate registry entry directly */
            result.fn_count++;
            std::string call_path = rec.is_async ? "futures::executor::block_on(" + pkg + "::" : pkg + "::";
            std::string call_close = rec.is_async ? ")" : "";
            std::string args_name = (rec.arg_count == 0) ? "_args" : "args";
            result.registry_entries +=
                "        m.insert(\"" + rec.name + "\".to_string(), |" + args_name + "| {\n";
            gen_deser_args(result.registry_entries, "            ", rec.arg_count, rec.args_str, rec.has_self, pkg);
            result.registry_entries += "            let result = " + call_path + rec.name + "(";
            for (int i = 0; i < rec.arg_count; i++) {
                if (i > 0) result.registry_entries += ", ";
                result.registry_entries += "a" + std::to_string(i);
            }
            result.registry_entries += ")" + call_close + ";\n";
            RetCap ret_cap = return_strategy(rec.return_type);
            bool is_ptr = is_raw_ptr_type(rec.return_type);
            if (rec.is_result_ret) {
                result.registry_entries += "            match result {\n";
                if (ret_cap == RET_DISPLAY || is_ptr) {
                    result.registry_entries += "                Ok(v) => Ok(serde_json::Value::String(v.to_string())),\n";
                } else if (ret_cap == RET_HANDLE) {
                    result.registry_entries += "                Ok(v) => Ok(serde_json::json!(Box::into_raw(Box::new(v)) as usize)),\n";
                } else {
                    result.registry_entries += "                Ok(v) => serde_json::to_value(v).map_err(|e| e.to_string()),\n";
                }
                result.registry_entries += "                Err(e) => Err(e.to_string()),\n";
                result.registry_entries += "            }\n";
            } else {
                if (ret_cap == RET_DISPLAY || is_ptr) {
                    result.registry_entries += "            Ok(serde_json::Value::String(result.to_string()))\n";
                } else if (ret_cap == RET_HANDLE) {
                    result.registry_entries += "            Ok(serde_json::json!(Box::into_raw(Box::new(result)) as usize))\n";
                } else {
                    result.registry_entries += "            Ok(serde_json::to_value(result).map_err(|e| e.to_string())?)\n";
                }
            }
            result.registry_entries += "        });\n";
        } else {
            /* In impl block */
            std::string crate_type = rec.impl_type;
            size_t ga = crate_type.find('<');
            bool type_had_generics = (ga != std::string::npos);
            if (ga != std::string::npos) crate_type = crate_type.substr(0, ga);
            size_t col = crate_type.rfind("::");
            if (col != std::string::npos) crate_type = crate_type.substr(col + 2);
            std::string safe_type = crate_type;
            FnRec mut_rec = rec; /* Mutable copy for optional concrete substitution */
            bool using_concrete_subst = false;

            /* Compute placeholder type for generic types (e.g. Mutex<T> → Mutex<()>) */
            std::string subst_type;
            if (type_had_generics) {
                subst_type = placeholder_type(rec.impl_type);
                if (subst_type.empty()) {
                    std::cout << "[bridge]     skip " << safe_type << "::" << rec.name << " (generic type)\n";
                    continue;
                }
                /* Check if this method references generic params in its signature */
                auto impl_params = extract_impl_type_params(rec.impl_type);
                /* Types whose generic params require trait bounds that () won't satisfy */
                static const char* bounded_generic_types[] = {
                    "Map", "HashMap", "BTreeMap", "IndexMap",
                    "Deserializer", "StreamDeserializer",
                    "LineColIterator", "IoRead", "SliceRead", "StrRead",
                    "Read", "Write",
                    "OccupiedEntry", "VacantEntry", "Entry",
                    "Iter", "IterMut", "IntoIter", "Keys", "Values", "ValuesMut",
                    "Serializer",
                    "PrettyFormatter", "CompactFormatter",
                    "Formatter",
                    /* rand crate types */
                    "DistIter", "Uniform", "WeightedIndex", "WeightedAliasIndex",
                    "Alphanumeric", "Standard", "Open01", "OpenClosed01",
                    /* regex crate types */
                    "Match", "Captures", "SubCaptureMatches", "Matches", "Split", "SplitN",
                    "NoExpand",
                };
                bool has_trait_bounds = false;
                for (auto* bt : bounded_generic_types) {
                    if (safe_type == bt) { has_trait_bounds = true; break; }
                }
                if (has_trait_bounds) {
                    std::string concrete = lookup_concrete_type(pkg, safe_type);
                    if (!concrete.empty()) {
                        /* Parse concrete values */
                        std::vector<std::string> concrete_vals;
                        {
                            std::istringstream cs(concrete);
                            std::string v;
                            while (std::getline(cs, v, ',')) {
                                v.erase(0, v.find_first_not_of(" \t\r\n"));
                                v.erase(v.find_last_not_of(" \t\r\n") + 1);
                                concrete_vals.push_back(v);
                            }
                        }
                        auto subst_in_fn = [&](std::string& s) {
                            for (size_t pi = 0; pi < impl_params.size() && pi < concrete_vals.size(); pi++) {
                                for (size_t p = 0; (p = s.find(impl_params[pi], p)) != std::string::npos; ) {
                                    char before = (p > 0) ? s[p - 1] : ' ';
                                    char after = (p + impl_params[pi].size() < s.size())
                                        ? s[p + impl_params[pi].size()] : ' ';
                                    if (!isalnum((unsigned char)before) && before != '_' &&
                                        !isalnum((unsigned char)after) && after != '_' &&
                                        after != ':' && before != ':') {
                                        s.replace(p, impl_params[pi].size(), concrete_vals[pi]);
                                        p += concrete_vals[pi].size();
                                    } else {
                                        p += impl_params[pi].size();
                                    }
                                }
                            }
                        };
                        /* Replace each () placeholder in subst_type with concrete params */
                        std::string new_subst = subst_type;
                        size_t npos = new_subst.find("()");
                        for (size_t ci = 0; ci < concrete_vals.size() && npos != std::string::npos; ci++) {
                            new_subst.replace(npos, 2, concrete_vals[ci]);
                            npos = new_subst.find("()", npos + concrete_vals[ci].size());
                        }
                        if (new_subst.find("()") != std::string::npos) {
                            std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                                      << " (unmatched concrete params)\n";
                        } else {
                            std::cout << "[bridge]     gen  " << safe_type << "::" << rec.name
                                      << " (concrete subst: " << new_subst << ")\n";
                            subst_type = new_subst;
                            subst_in_fn(mut_rec.args_str);
                            subst_in_fn(mut_rec.return_type);
                            if (!method_references_generic_params(mut_rec.args_str, mut_rec.return_type, impl_params))
                                using_concrete_subst = true;
                            else
                                std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                                          << " (concrete subst but still has generic refs)\n";
                        }
                    }
                    if (!using_concrete_subst) {
                        std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                                  << " (bounded generic type)\n";
                        continue;
                    }
                } else {
                    /* Non-bounded generic: check if method references generic params */
                    auto impl_params = extract_impl_type_params(rec.impl_type);
                    if (method_references_generic_params(rec.args_str, rec.return_type, impl_params)) {
                        std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                                  << " (generic params in signature)\n";
                        continue;
                    }
                    std::cout << "[bridge]     gen  " << subst_type << "::" << rec.name << " (placeholder generics)\n";
                }
            }
            /* Skip methods that take self by value (can't use with &this_ref) */
            {
                const auto& args_to_check = using_concrete_subst ? mut_rec.args_str : rec.args_str;
                if (rec.has_self && takes_self_by_value(args_to_check)) {
                    std::cout << "[bridge]     skip " << safe_type << "::" << rec.name << " (self by value)\n";
                    continue;
                }
            }

            /* Skip concrete types in private sub-modules that can't be named at crate root */
            {
                static const char* inaccessible_concrete_types[] = {
                    "RawValue", "IoErrorKind", "ErrorKind",
                };
                bool skip_inaccessible = false;
                for (auto* ct : inaccessible_concrete_types) {
                    if (safe_type == ct) { skip_inaccessible = true; break; }
                }
                if (skip_inaccessible) {
                    std::cout << "[bridge]     skip " << safe_type << "::" << rec.name
                              << " (inaccessible type)\n";
                    continue;
                }
            }

            if (!types.count(safe_type)) {
                types[safe_type] = {crate_type, safe_type, subst_type, {}, {}};
            }
            if (rec.has_self) {
                types[safe_type].methods.push_back(using_concrete_subst ? mut_rec : rec);
            } else {
                types[safe_type].ctors.push_back(using_concrete_subst ? mut_rec : rec);
            }
        }
    }

    /* Generate type-based registries */
    for (auto& [safe, bucket] : types) {
        const std::string& crate_type = bucket.crate_type;
        std::string type_ref = bucket.subst_type.empty() ? crate_type : bucket.subst_type;
        /* Fix sub-module type paths (e.g. RawValue → serde_json::raw::RawValue) */
        type_ref = fix_module_path(type_ref, pkg);
        result.method_count += (int)bucket.methods.size() + (int)bucket.ctors.size();

        /* Method registry init (for type_registry) */
        if (!bucket.methods.empty()) {
            result.method_registry_init += "    // ── " + type_ref + " methods ──\n";
            result.method_registry_init += "    {\n";
            result.method_registry_init += "        let tn = \"" + safe + "\".to_string();\n";
            result.method_registry_init += "        let mut methods: HashMap<String, MethodFn> = HashMap::new();\n";
            for (const auto& m : bucket.methods) {
                std::string m_args = (m.arg_count == 0) ? "_args" : "args";
                result.method_registry_init += "        methods.insert(\"" + m.name + "\".to_string(), |this_ptr: *mut c_void, " + m_args + ": Vec<serde_json::Value>| -> std::result::Result<serde_json::Value, String> {\n";
                bool m_self_mut = m.has_self && (m.args_str.find("&mut self") != std::string::npos);
                result.method_registry_init += "            let this_ref = unsafe { &" + std::string(m_self_mut ? "mut " : "") + "*(this_ptr as *" + (m_self_mut ? "mut" : "const") + " " + type_ref + ") };\n";
                gen_deser_args(result.method_registry_init, "            ", m.arg_count, m.args_str, m.has_self, pkg);
                std::string ap = m.is_async ? "futures::executor::block_on(" : "";
                std::string as_ = m.is_async ? ")" : "";
                result.method_registry_init += "            let __result = " + ap + "this_ref." + m.name + "(";
                for (int i = 0; i < m.arg_count; i++) {
                    if (i > 0) result.method_registry_init += ", ";
                    result.method_registry_init += "a" + std::to_string(i);
                }
                result.method_registry_init += ")" + as_ + ";\n";
                RetCap m_ret_cap = return_strategy(m.return_type);
                bool m_is_ptr = is_raw_ptr_type(m.return_type);
                bool m_is_self_ref = is_self_ref_type(m.return_type);
                if (m.is_result_ret) {
                    result.method_registry_init += "            match __result {\n";
                    if (m_ret_cap == RET_DISPLAY || m_is_ptr || m_is_self_ref) {
                        result.method_registry_init += "                Ok(v) => Ok(serde_json::Value::String(v.to_string())),\n";
                    } else if (m_ret_cap == RET_HANDLE) {
                        result.method_registry_init += "                Ok(v) => Ok(serde_json::json!(Box::into_raw(Box::new(v)) as usize)),\n";
                    } else {
                        result.method_registry_init += "                Ok(v) => serde_json::to_value(&v).map_err(|e| e.to_string()),\n";
                    }
                    result.method_registry_init += "                Err(e) => Err(e.to_string()),\n";
                    result.method_registry_init += "            }\n";
                } else {
                    if (m_ret_cap == RET_DISPLAY || m_is_ptr || m_is_self_ref) {
                        result.method_registry_init += "            Ok(serde_json::Value::String(__result.to_string()))\n";
                    } else if (m_ret_cap == RET_HANDLE) {
                        result.method_registry_init += "            Ok(serde_json::json!(Box::into_raw(Box::new(__result)) as usize))\n";
                    } else {
                        result.method_registry_init += "            serde_json::to_value(&__result).map_err(|e| e.to_string())\n";
                    }
                }
                result.method_registry_init += "        });\n";
            }
            result.method_registry_init += "        type_map.insert(tn, methods);\n";
            result.method_registry_init += "    }\n";
        }

        /* Constructor registry init (for ctor_registry) */
        if (!bucket.ctors.empty()) {
            result.ctor_registry_init += "    // ── " + type_ref + " constructors ──\n";
            result.ctor_registry_init += "    {\n";
            result.ctor_registry_init += "        let tn = \"" + safe + "\".to_string();\n";
            result.ctor_registry_init += "        let mut ctors: HashMap<String, CtorFn> = HashMap::new();\n";
            for (const auto& c : bucket.ctors) {
                std::string c_args_name = (c.arg_count == 0) ? "_args" : "args";
                result.ctor_registry_init += "        ctors.insert(\"" + c.name + "\".to_string(), |" + c_args_name + ": Vec<serde_json::Value>| -> std::result::Result<*mut c_void, String> {\n";
                gen_deser_args(result.ctor_registry_init, "            ", c.arg_count, c.args_str, c.has_self, pkg);
                std::string ap2 = c.is_async ? "futures::executor::block_on(" : "";
                std::string as2 = c.is_async ? ")" : "";
                result.ctor_registry_init += "            let __val = " + ap2 + turbofish_type(type_ref) + "::" + c.name + "(";
                for (int i = 0; i < c.arg_count; i++) {
                    if (i > 0) result.ctor_registry_init += ", ";
                    result.ctor_registry_init += "a" + std::to_string(i);
                }
                result.ctor_registry_init += ")" + as2 + ";\n";
                if (c.is_result_ret) {
                    result.ctor_registry_init += "            match __val {\n";
                    result.ctor_registry_init += "                Ok(v) => Ok(Box::into_raw(Box::new(v)) as *mut c_void),\n";
                    result.ctor_registry_init += "                Err(e) => Err(e.to_string()),\n";
                    result.ctor_registry_init += "            }\n";
                } else {
                    result.ctor_registry_init += "            Ok(Box::into_raw(Box::new(__val)) as *mut c_void)\n";
                }
                result.ctor_registry_init += "        });\n";
            }
            result.ctor_registry_init += "        ctor_map.insert(tn, ctors);\n";
            result.ctor_registry_init += "    }\n";
        }

        /* Drop registry init */
        result.drop_registry_init += "    m.insert(\"" + safe + "\".to_string(), {\n";
        result.drop_registry_init += "        let f: DropFn = |ptr| unsafe { drop(Box::from_raw(ptr as *mut " + type_ref + ")); };\n";
        result.drop_registry_init += "        f\n";
        result.drop_registry_init += "    });\n";

        /* .au entries */
        for (const auto& m : bucket.methods) {
            result.method_au_entries += "@cost(alloc)\n";
            result.method_au_entries += "extern \"cargo_" + pkg + "\" function "
                + pkg + "_" + safe + "_" + m.name
                + "(this: pointer, args: pointer) -> pointer\n";
        }
        for (const auto& c : bucket.ctors) {
            result.method_au_entries += "@cost(alloc)\n";
            result.method_au_entries += "extern \"cargo_" + pkg + "\" function "
                + pkg + "_" + safe + "_" + c.name
                + "(args: pointer) -> pointer\n";
        }
    }

    int total = result.fn_count + result.method_count;
    if (total > 0) {
        std::cout << "[bridge]   auto-discovered " << result.fn_count
                  << " free functions, " << result.method_count << " methods/constructors\n";
    } else {
        std::cout << "[bridge]   WARNING: no bridgeable functions found (check for cfg/platform filters)\n";
    }

    /* Cleanup temp files */
    fs::remove(tarball_path);
    fs::remove_all(extract_dir);
    return result;
}

