; Win64 FFI call dispatcher
;   ffi_call_impl(cif, fn, ret, args)
;   RCX = cif         (ffi_cif* — reads ret_type at [rcx+0], arg_count at [rcx+4])
;   RDX = fn          (function pointer to call)
;   R8  = ret         (return value buffer)
;   R9  = args        (ffi_value[]: [0..3] = reg vals, [4..] = stack vals)
;
; Non-volatile registers used:
;   R13 = cif, R14 = stack_count, R15 = saved RAX (return value)

.code

ffi_call_impl PROC
    push    r13
    push    r14
    push    r15
    mov     r13, rcx          ; cif pointer
    mov     r10, rdx          ; fn
    mov     r11, r8           ; ret buffer
    mov     r12, r9           ; args array

    ; ── Load first 4 args into integer registers ──
    mov     rcx, [r12 + 0*8]
    mov     rdx, [r12 + 1*8]
    mov     r8,  [r12 + 2*8]
    mov     r9,  [r12 + 3*8]

    ; ── Load XMM registers from same slots ──
    movsd   xmm0, qword ptr [r12 + 0*8]
    movsd   xmm1, qword ptr [r12 + 1*8]
    movsd   xmm2, qword ptr [r12 + 2*8]
    movsd   xmm3, qword ptr [r12 + 3*8]

    ; ── Calculate stack args count: max(0, arg_count - 4) ──
    mov     eax, dword ptr [r13 + 4]     ; cif->arg_count
    sub     eax, 4
    jle     alloc_only                   ; if <= 4 reg args, skip stack copy
    mov     r14, rax                     ; r14 = stack_count
    jmp     alloc

alloc_only:
    xor     r14, r14                     ; stack_count = 0

alloc:
    ; Allocate shadow space + space for stack args (16-aligned)
    ; Need: 20h (shadow) + r14*8 (stack args) + padding to 16 bytes
    ; Simple: allocate 20h + max(8*16, r14*8) = 20h + 80h = A0h
    sub     rsp, 0A0h

    ; ── Copy stack args to [rsp+20h..] ──
    xor     r15d, r15d                   ; index = 0
copy_loop:
    cmp     r15, r14
    jae     copy_done
    mov     rax, [r12 + 4*8 + r15*8]
    mov     [rsp + 20h + r15*8], rax
    inc     r15d
    jmp     copy_loop
copy_done:

    ; ── Call the function ──
    call    r10

    ; ── Save return values ──
    mov     r15, rax                     ; save integer return
    movsd   qword ptr [rsp + 8], xmm0    ; save float return to shadow space

    ; ── Store return value based on ret_type ──
    mov     eax, dword ptr [r13]         ; cif->ret_type
    cmp     eax, 9                       ; FFI_FLOAT
    je      float_ret
    cmp     eax, 10                      ; FFI_DOUBLE
    je      float_ret
    cmp     eax, 0                       ; FFI_VOID
    je      done
    ; integer/ptr
    mov     [r11], r15
    jmp     done
float_ret:
    movsd   xmm0, qword ptr [rsp + 8]    ; restore saved xmm0
    movsd   qword ptr [r11], xmm0
done:
    add     rsp, 0A0h
    pop     r15
    pop     r14
    pop     r13
    ret

ffi_call_impl ENDP

END
