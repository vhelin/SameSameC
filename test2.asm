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
      ; TAC: variable "var" size 1 offset 0 type n
      ; -------------------------------------------------------
      ; TAC: r0.int16 (int16) := g_value.int8 (int16) + 1.uint8 (int16)
      LD  IX,g_value
      LD  L,(IX+0)
      ; sign extend 8-bit L -> 16-bit HL
      LD  A,L
      ADD A,A  ; sign bit of A into carry
      SBC A,A  ; A = 0 if carry == 0, $FF otherwise
      LD  H,A  ; now HL is sign extended L
      LD  BC,1
      ADD HL,BC
      ; offset -1
      LD  IX,-1
      ADD IX,DE
      LD  (IX+0),L
      LD  (IX+1),H
      ; -------------------------------------------------------
      ; TAC: r1.int16 (int16) := r0.int16 (int16) + 10.uint8 (int16)
      ; offset -1
      LD  IX,-1
      ADD IX,DE
      LD  L,(IX+0)
      LD  H,(IX+1)
      LD  BC,10
      ADD HL,BC
      ; offset -3
      LD  IX,-3
      ADD IX,DE
      LD  (IX+0),L
      LD  (IX+1),H
      ; -------------------------------------------------------
      ; TAC: r2.int16 (int16) := r1.int16 (int16) + 100.uint8 (int16)
      ; offset -3
      LD  IX,-3
      ADD IX,DE
      LD  L,(IX+0)
      LD  H,(IX+1)
      LD  BC,100
      ADD HL,BC
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      LD  (IX+0),L
      LD  (IX+1),H
      ; -------------------------------------------------------
      ; TAC: var.int8 (int8) := r2.int16 (int16) + 1000.int16 (int16)
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      LD  L,(IX+0)
      LD  H,(IX+1)
      LD  BC,1000
      ADD HL,BC
      ; offset 0
      LD  IX,0
      ADD IX,DE
      LD  (IX+0),L
      ; -------------------------------------------------------
      ; TAC: return
  .ENDS
