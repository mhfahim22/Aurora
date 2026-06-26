/* Test if PyUnicode_AsUTF8 is exported from the installed Python DLL */
#include <cstdio>
#include <windows.h>

int main() {
    /* Try stable forwarder first, then versioned DLLs */
    const char* candidates[] = {
        "python3.dll", "python314.dll", "python313.dll", "python312.dll",
        "python311.dll", "python310.dll", NULL
    };
    HMODULE h = NULL;
    const char* loaded = NULL;
    for (int i = 0; candidates[i]; i++) {
        h = LoadLibraryA(candidates[i]);
        if (h) { loaded = candidates[i]; break; }
    }
    if (!h) { printf("FAIL: could not load any Python DLL\n"); return 1; }
    printf("Loaded: %s\n", loaded);

    void* p1 = GetProcAddress(h, "PyUnicode_AsUTF8");
    void* p2 = GetProcAddress(h, "PyUnicode_AsUTF8AndSize");
    void* p3 = GetProcAddress(h, "PyImport_ImportModule");

    printf("PyUnicode_AsUTF8:       %p\n", p1);
    printf("PyUnicode_AsUTF8AndSize: %p\n", p2);
    printf("PyImport_ImportModule:   %p\n", p3);

    printf("AsUTF8 available:  %s\n", p1 ? "YES" : "NO");
    printf("AsUTF8AndSize available: %s\n", p2 ? "YES" : "NO");

    /* Report findings — symbols may not be in stable ABI (python3.dll)
       while being present in version-specific DLLs. Non-fatal. */
    if (!p1) printf("NOTE: PyUnicode_AsUTF8 not found (not in stable ABI on this Python)\n");
    return 0;
}
