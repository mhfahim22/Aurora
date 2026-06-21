#include "bridge_shared.h"

std::string find_native_dll(const std::string& name) {
    /* Try name as-is */
    if (fs::exists(name)) return fs::absolute(name).string();

    /* Try with .dll/.so extension */
#ifdef _WIN32
    std::string with_ext = name + ".dll";
    if (fs::exists(with_ext)) return fs::absolute(with_ext).string();

    /* Search System32 and SysWOW64 */
    char sysdir[MAX_PATH];
    if (GetSystemDirectoryA(sysdir, sizeof(sysdir))) {
        std::string syspath = std::string(sysdir) + "\\" + with_ext;
        if (fs::exists(syspath)) return syspath;
    }
    /* Also search PATH */
    char* path_env = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&path_env, &sz, "PATH") == 0 && path_env) {
        std::string path_str(path_env);
        free(path_env);
        std::istringstream ss(path_str);
        std::string dir;
        while (std::getline(ss, dir, ';')) {
            if (dir.empty()) continue;
            std::string candidate = dir + "\\" + with_ext;
            if (fs::exists(candidate)) return fs::absolute(candidate).string();
        }
    }
#else
    std::string with_ext = "lib" + name + ".so";
    if (fs::exists(with_ext)) return fs::absolute(with_ext).string();
    with_ext = "lib" + name + ".dylib";
    if (fs::exists(with_ext)) return fs::absolute(with_ext).string();
    with_ext = name + ".so";
    if (fs::exists(with_ext)) return fs::absolute(with_ext).string();

    /* Search LD_LIBRARY_PATH */
    const char* ld_path = getenv("LD_LIBRARY_PATH");
    if (ld_path) {
        std::string path_str(ld_path);
        std::istringstream ss(path_str);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (dir.empty()) continue;
            std::string candidate = dir + "/lib" + name + ".so";
            if (fs::exists(candidate)) return fs::absolute(candidate).string();
        }
    }
#endif

    /* Search current directory */
    std::string cwd = fs::current_path().string();
#ifdef _WIN32
    std::string cwd_candidate = cwd + "\\" + name + ".dll";
#else
    std::string cwd_candidate = cwd + "/lib" + name + ".so";
#endif
    if (fs::exists(cwd_candidate)) return fs::absolute(cwd_candidate).string();

    return "";
}

std::vector<std::string> get_dll_exports(const std::string& dll_path) {
    std::vector<std::string> exports;
#ifdef _WIN32
    /* Open the DLL file and parse the PE export directory directly */
    HANDLE hFile = CreateFileA(dll_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return exports;

    DWORD fsize = GetFileSize(hFile, nullptr);
    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) { CloseHandle(hFile); return exports; }

    const uint8_t* base = (const uint8_t*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!base) { CloseHandle(hMap); CloseHandle(hFile); return exports; }

    /* Parse DOS header */
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { UnmapViewOfFile(base); CloseHandle(hMap); CloseHandle(hFile); return exports; }

    /* Parse PE header */
    const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) { UnmapViewOfFile(base); CloseHandle(hMap); CloseHandle(hFile); return exports; }

    /* Build RVA → file offset mapping from section headers */
    WORD num_sections = nt->FileHeader.NumberOfSections;
    DWORD section_offset = dos->e_lfanew + 4 /* PE signature */
                         + sizeof(IMAGE_FILE_HEADER)
                         + nt->FileHeader.SizeOfOptionalHeader;
    const IMAGE_SECTION_HEADER* sections = (const IMAGE_SECTION_HEADER*)(base + section_offset);
    auto rva_to_offset = [&](DWORD rva) -> DWORD {
        if (rva < nt->OptionalHeader.SizeOfHeaders) return rva;
        for (WORD i = 0; i < num_sections; i++) {
            DWORD start = sections[i].VirtualAddress;
            DWORD end = start + sections[i].SizeOfRawData;
            if (rva >= start && rva < end) {
                return rva - start + sections[i].PointerToRawData;
            }
        }
        return rva; /* fallback */
    };

    /* Validate section_offset before using sections pointer */
    if (section_offset > fsize - sizeof(IMAGE_SECTION_HEADER) * num_sections) {
        UnmapViewOfFile(base); CloseHandle(hMap); CloseHandle(hFile); return exports;
    }
    /* Get export directory (convert RVA to file offset) */
    DWORD export_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (export_rva == 0) { UnmapViewOfFile(base); CloseHandle(hMap); CloseHandle(hFile); return exports; }
    DWORD export_fo = rva_to_offset(export_rva);
    const IMAGE_EXPORT_DIRECTORY* ed = (const IMAGE_EXPORT_DIRECTORY*)(base + export_fo);

    /* Get name pointer table */
    DWORD names_rva = ed->AddressOfNames;
    DWORD names_fo = rva_to_offset(names_rva);
    const DWORD* name_ptr = (const DWORD*)(base + names_fo);

    DWORD num_names = ed->NumberOfNames;
    exports.reserve(num_names);

    for (DWORD i = 0; i < num_names; i++) {
        DWORD name_rva = name_ptr[i];
        DWORD name_fo = rva_to_offset(name_rva);
        if (name_fo >= fsize) continue;
        const char* fn_name = (const char*)(base + name_fo);
        if (!fn_name || !*fn_name) continue;
        if (fn_name[0] == '.' || fn_name[0] == '?' || fn_name[0] == '@') continue;
        exports.push_back(fn_name);
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    CloseHandle(hFile);
#else
    /* Use nm -D to list dynamic symbols */
    std::string cmd = "nm -D \"" + dll_path + "\" 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return exports;
    char line[1024];
    while (fgets(line, sizeof(line), pipe)) {
        /* Format: address T name */
        char type = 0;
        char fname[512] = {0};
        unsigned long addr = 0;
        if (sscanf(line, "%lx %c %511s", &addr, &type, fname) >= 3) {
            if (type == 'T' || type == 't') {
                exports.push_back(fname);
            }
        }
    }
    pclose(pipe);
#endif
    /* Sort and deduplicate */
    std::sort(exports.begin(), exports.end());
    exports.erase(std::unique(exports.begin(), exports.end()), exports.end());
    return exports;
}

void gen_native_au_binding(const std::string& pkg, const std::string& dll_path,
                            const std::vector<std::string>& exports, std::ostream& os)
{
    std::string dll_name = fs::path(dll_path).filename().string();

    os << "/* ════════════════════════════════════════════════════════════\n";
    os << "   Native DLL Bridge — Auto-generated Aurora FFI Bindings\n";
    os << "   Library: " << dll_name << "\n";
    os << "   Path: " << dll_path << "\n";
    os << "   ════════════════════════════════════════════════════════════ */\n\n";

    if (exports.empty()) {
        os << "/* ⚠ No exports discovered. Add function declarations manually:\n";
        os << "   @cost(zero)\n";
        os << "   extern \"native_" << pkg << "\" function MyFunction(arg: i32) -> i32\n";
        os << "*/\n\n";
        os << "@cost(zero)\n";
        os << "extern \"native_" << pkg << "\" function DllMain() -> i32\n";
        return;
    }

    os << "/* " << exports.size() << " exported functions */\n\n";

    for (const auto& fn : exports) {
        if (fn.empty() || fn[0] == '?' || fn[0] == '.' || fn[0] == '_' || fn[0] == '@')
            continue;
        if (fn.rfind("__", 0) == 0) continue;
        if (fn.rfind("_Crt", 0) == 0) continue;

        os << "@cost(zero)\n";
        os << "extern \"native_" << pkg << "\" function " << fn << "() -> pointer\n";
    }
}
