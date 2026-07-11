#ifdef __GNUC__
#include <cstdint>
#include <cstring>

struct ffi_cif {
    int ret_type;
    int arg_count;
    int arg_types[16];
};

typedef union ffi_value {
    int8_t    i8;
    int16_t   i16;
    int32_t   i32;
    int64_t   i64;
    uint8_t   u8;
    uint16_t  u16;
    uint32_t  u32;
    uint64_t  u64;
    float     f32;
    double    f64;
    void*     ptr;
} ffi_value;

extern "C" void ffi_call_impl(const ffi_cif* cif, void* fn,
                              ffi_value* ret, ffi_value* args) {
    uint64_t a0 = args ? args[0].u64 : 0;
    uint64_t a1 = args ? args[1].u64 : 0;
    uint64_t a2 = args ? args[2].u64 : 0;
    uint64_t a3 = args ? args[3].u64 : 0;

    int stack_count = 0;
    if (cif && cif->arg_count > 4) {
        stack_count = cif->arg_count - 4;
    }

    uint64_t result_int = 0;
    double result_dbl = 0.0;

    if (stack_count > 0 && args) {
        uint64_t* sp = new uint64_t[stack_count];
        for (int i = 0; i < stack_count; i++) {
            sp[i] = args[4 + i].u64;
        }
        __asm__ __volatile__ (
            "movq    %[a0], %%rcx\n"
            "movq    %[a1], %%rdx\n"
            "movq    %[a2], %%r8\n"
            "movq    %[a3], %%r9\n"
            "movsd   %[a0], %%xmm0\n"
            "movsd   %[a1], %%xmm1\n"
            "movsd   %[a2], %%xmm2\n"
            "movsd   %[a3], %%xmm3\n"
            "subq    $0x28, %%rsp\n"
            "movl    %[sc], %%r10d\n"
            "xorl    %%eax, %%eax\n"
            "0:\n"
            "cmpl    %%eax, %%r10d\n"
            "jbe     1f\n"
            "movq    (%[sp],%%rax,8), %%r11\n"
            "movq    %%r11, 0x20(%%rsp,%%rax,8)\n"
            "incl    %%eax\n"
            "jmp     0b\n"
            "1:\n"
            "call    *%[fn]\n"
            "addq    $0x28, %%rsp\n"
            "movq    %%rax, %[ri]\n"
            "movsd   %%xmm0, %[rd]\n"
            : [ri] "=m" (result_int),
              [rd] "=m" (result_dbl)
            : [fn] "r" (fn),
              [sp] "r" (sp),
              [sc] "r" (stack_count),
              [a0] "m" (a0),
              [a1] "m" (a1),
              [a2] "m" (a2),
              [a3] "m" (a3)
            : "%rcx", "%rdx", "%r8", "%r9",
              "%r10", "%r11",
              "%xmm0", "%xmm1", "%xmm2", "%xmm3",
              "memory", "cc"
        );
        delete[] sp;
    } else {
        __asm__ __volatile__ (
            "movq    %[a0], %%rcx\n"
            "movq    %[a1], %%rdx\n"
            "movq    %[a2], %%r8\n"
            "movq    %[a3], %%r9\n"
            "movsd   %[a0], %%xmm0\n"
            "movsd   %[a1], %%xmm1\n"
            "movsd   %[a2], %%xmm2\n"
            "movsd   %[a3], %%xmm3\n"
            "subq    $0x20, %%rsp\n"
            "call    *%[fn]\n"
            "addq    $0x20, %%rsp\n"
            "movq    %%rax, %[ri]\n"
            "movsd   %%xmm0, %[rd]\n"
            : [ri] "=m" (result_int),
              [rd] "=m" (result_dbl)
            : [fn] "r" (fn),
              [a0] "m" (a0),
              [a1] "m" (a1),
              [a2] "m" (a2),
              [a3] "m" (a3)
            : "%rcx", "%rdx", "%r8", "%r9",
              "%xmm0", "%xmm1", "%xmm2", "%xmm3",
              "memory", "cc"
        );
    }

    if (cif && ret) {
        switch (cif->ret_type) {
            case 0:  break;
            case 9:
            case 10: ret->f64 = result_dbl; break;
            default: ret->u64 = result_int; break;
        }
    }
}

#endif
