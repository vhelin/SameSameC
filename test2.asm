  .SECTION "main" FREE
    main:
      ; A  - tmp
      ; BC - tmp
      ; DE - frame pointer
      ; HL - tmp
      ; SP - stack pointer
      ; IX - tmp
      ; IY - tmp
      ; -------------------------------------------------------
      ; TAC: variable "var" size 2 offset 0 type n
      ; -------------------------------------------------------
      ; TAC: var.w := g_value.b
      ; offset 0
      LD  IX,0
      ADD IX,DE
      LD  BC,0
      ADD IX,BC
      LD  IY,g_value
      LD  A,(IY+0)
      ; sign extend 8-bit A -> 16-bit BC
      LD  C,A
      ADD A,A  ; sign bit of A into carry
      SBC A,A  ; A = 0 if carry == 0, $FF otherwise
      LD  B,A  ; now BC is sign extended A
      LD  (IX+0),C
      LD  (IX+1),B
      ; -------------------------------------------------------
      ; TAC: return
  .ENDS
