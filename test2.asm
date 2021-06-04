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
      ; TAC: r0.int16 (int16) := g_value.int8 (int16) + 1.uint8 (int16)
      LD  IY,g_value
      LD  C,(IY+0)
      ; sign extend 8-bit C -> 16-bit BC
      LD  A,C
      ADD A,A  ; sign bit of A into carry
      SBC A,A  ; A = 0 if carry == 0, $FF otherwise
      LD  B,A  ; now BC is sign extended A
      LD  HL,1
      ADD HL,BC
      ; offset -2
      LD  IX,-2
      ADD IX,DE
      ; -------------------------------------------------------
      ; TAC: r1.int16 (int16) := r0.int16 (int16) + 10.uint8 (int16)
      ; offset -2
      LD  IY,-2
      ADD IY,DE
      LD  C,(IY+0)
      LD  B,(IY+1)
      LD  HL,10
      ADD HL,BC
      ; offset -4
      LD  IX,-4
      ADD IX,DE
      ; -------------------------------------------------------
      ; TAC: r2.int16 (int16) := r1.int16 (int16) + 100.uint8 (int16)
      ; offset -4
      LD  IY,-4
      ADD IY,DE
      LD  C,(IY+0)
      LD  B,(IY+1)
      LD  HL,100
      ADD HL,BC
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      ; -------------------------------------------------------
      ; TAC: var.int16 (int16) := r2.int16 (int16) + 1000.int16 (int16)
      ; offset -6
      LD  IY,-6
      ADD IY,DE
      LD  C,(IY+0)
      LD  B,(IY+1)
      LD  HL,1000
      ADD HL,BC
      ; offset 0
      LD  IX,0
      ADD IX,DE
      ; -------------------------------------------------------
      ; TAC: return
  .ENDS
