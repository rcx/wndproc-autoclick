include ksamd64.inc

EXTERNDEF __imp_RtlCaptureContext:QWORD
extern HookCallback:proc
extern savedContext:QWORD

.code
     
Trampoline64 PROC
    push rcx                            ; preserve rcx
    lea rcx, qword ptr [savedContext]
    call __imp_RtlCaptureContext        ; assume stack is already aligned

    pop [rcx+CxRcx]                       ; 0x80 = offset of rcx

    lea rax, [rsp]                  ; calculate original rsp
    mov [rcx+CxRsp], rax                  ; 0x98 = offset of rsp

    sub rsp, 20h                        ; shadow space
    jmp HookCallback
Trampoline64 ENDP
     
end