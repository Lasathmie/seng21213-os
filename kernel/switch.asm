; =============================================================================
; void context_switch(uint32_t *old_esp, uint32_t new_esp)
;
; Saves the outgoing process's 8 GPRs and ESP into *old_esp, loads the
; incoming process's ESP, restores its 8 GPRs, and returns into it.
;
; This is deliberately symmetric: the SAME pushad/popad/ret shape is used
; whether the target is a process that's run before (its saved esp points
; at a frame this function itself built, last time it was switched out)
; or a brand-new one (process_create() builds a frame in this exact shape:
; a fake return address = entry point, sitting on top of 8 zeroed regs).
; Mixing this with an IRET-style frame is what causes a triple fault the
; first time you resume an already-run process.
; =============================================================================

[BITS 32]
[GLOBAL context_switch]

context_switch:
    pushad                   ; save EAX ECX EDX EBX ESP EBP ESI EDI
    mov  eax, [esp + 36]     ; arg0: old_esp ptr  (36 = 8 regs x 4 + ret addr)
    mov  [eax], esp          ; *old_esp = current ESP
    mov  esp, [esp + 40]     ; arg1: new_esp — switch stacks
    popad                    ; restore next process's registers
    sti                      ; always leave with interrupts on, so the timer
                             ; keeps firing regardless of which process this is
    ret                      ; return address on top of new stack: for a
                             ; resumed process, back into scheduler_tick();
                             ; for a fresh one, straight into entry()