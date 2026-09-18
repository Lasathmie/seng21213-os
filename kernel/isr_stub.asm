; =============================================================================
; This stub is registered in the IDT for vector 32 (IRQ0 after PIC remap).
; The CPU automatically pushes EFLAGS, CS, EIP when the interrupt fires.
; We push all general-purpose registers with PUSHAD, call the C scheduler,
; then restore with POPAD and return with IRET.
;
; IMPORTANT: We do NOT call pic_send_eoi here directly — the C scheduler
; calls it via a helper after the context switch logic completes.
;
;DEBUG COUNT: 3
;
; =============================================================================

[BITS 32]
[EXTERN scheduler_tick]
[EXTERN pic_send_eoi]
[GLOBAL irq0_handler]
[GLOBAL irq1_handler]
[GLOBAL irq0_post_switch]   ;exported so process.c can push it as return addr

irq0_handler:
    pushad                  ; save EAX ECX EDX EBX ESP EBP ESI EDI

    call scheduler_tick     ; C function — does the round-robin + context_switch

irq0_post_switch:           ; context_switch returns HERE for new processes

    			    ; Send EOI to master PIC (IRQ0 is on master, so no slave EOI needed)
    			    ; We push 0 as the irq argument to pic_send_eoi(0)
    push  byte 0
    call  pic_send_eoi
    add   esp, 4            ; clean up argument

    popad                   ; restore registers
    iret                    ; pop EIP, CS, EFLAGS — resumes process

irq1_handler:
    pushad
    push byte 1             ; irq number = 1
    call pic_send_eoi
    add esp, 4

    popad
    iret