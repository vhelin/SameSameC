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
      ; test2.blb:7: int16 var = g_i[20+g_value];
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
      ; TAC: r1.int16 (int16) := g_i.int16[r0.uint8 (uint8)]
      ; -----------------------------------------------------------------
      ; -----------------------------------------------------------------
      ; TAC: var.int16 (int16) := r1.int16 (int16)
      ; -----------------------------------------------------------------
      ; offset 0
      LD  IX,0
      ADD IX,DE
      ; offset -3
      LD  IY,-3
      ADD IY,DE
      LD  A,(IY+0)
      LD  (IX+0),A
      LD  A,(IY+1)
      LD  (IX+1),A
      ; -----------------------------------------------------------------
      ; TAC: return
      ; -----------------------------------------------------------------
  .ENDS
