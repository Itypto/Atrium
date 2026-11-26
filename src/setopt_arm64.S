    .text
    .p2align 2
    .global atrium_setopt_hook
    .hidden atrium_setopt_hook
    .type atrium_setopt_hook, %function
atrium_setopt_hook:
    mov w9, #10002
    cmp w1, w9
    b.eq atrium_handle_setopt
    mov w9, #10065
    cmp w1, w9
    b.eq atrium_handle_setopt
    mov w9, #20108
    cmp w1, w9
    b.eq atrium_handle_setopt
    cmp w1, #3
    b.eq atrium_handle_setopt
    cmp w1, #64
    b.eq atrium_handle_setopt
    cmp w1, #81
    b.eq atrium_handle_setopt
    adrp x16, atrium_original_setopt
    ldr x16, [x16, :lo12:atrium_original_setopt]
    br x16
    .size atrium_setopt_hook, .-atrium_setopt_hook
