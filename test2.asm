  ; =================================================================
  ; test2.blb:5: void main() {
  ; =================================================================
  .SECTION "main" FREE
    main:
      ; A  - tmp
      ; BC - tmp
      ; DE - frame pointer
      ; HL - tmp
      ; SP - stack pointer
      ; IX - tmp
      ; IY - tmp
      ; =================================================================
      ; test2.blb:7: int16 var = &g_i[20+g_value];
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: variable "var" size 2 offset 0 type n
      ; -----------------------------------------------------------------
      ; -----------------------------------------------------------------
      ; TAC: r0.uint8 (uint8) := 20.uint8 (uint8) + g_value.int8 (uint8)
      ; -----------------------------------------------------------------
      LD  A,20
      LD  IX,g_value
      ADD A,(IX+0)
      ; offset -2
      LD  IX,-2
      ADD IX,DE
      LD  (IX+0),A
      ; -----------------------------------------------------------------
      ; TAC: var.int16 (int16) := &g_i.uint16 (uint16)[r0.uint8 (uint8)]
      ; -----------------------------------------------------------------
      LD  HL,g_i
      ; offset -2
      LD  IX,-2
      ADD IX,DE
      LD  C,(IX+0)
      LD  B,0
      ADD HL,BC
      ; offset 0
      LD  IX,0
      ADD IX,DE
      LD  (IX+0),L
      LD  (IX+1),H
      ; -----------------------------------------------------------------
      ; TAC: return
      ; -----------------------------------------------------------------
  .ENDS
