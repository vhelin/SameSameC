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
      ; test2.blb:7: int16 a = 99 % g_value;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: variable "a" size 2 offset 0 type n
      ; -----------------------------------------------------------------
      ; -----------------------------------------------------------------
      ; TAC: a.int16 (int16) := 99.uint8 (uint8) % g_value.int8 (uint8)
      ; -----------------------------------------------------------------
      LD  H,99
      LD  IX,g_value
      PUSH DE
      LD  E,(IX+0)
      ; divide H / E -> A (result) & B (remainder)
      XOR A
      LD  B,8
    -
      RL  H
      RLA
      SUB E
      JR  NC,+
      ADD A,E
    +
      DJNZ -
      LD  B,A
      LD  A,H
      RLA
      CPL
      POP DE
      ; offset 0
      LD  IX,0
      ADD IX,DE
      LD  (IX+0),B
      ; sign extend 8-bit B -> (IX)
      LD  A,B
      ADD A,A  ; sign bit of A into carry
      SBC A,A  ; A = 0 if carry == 0, $FF otherwise
      LD  (IX+1),A
      ; -----------------------------------------------------------------
      ; TAC: return
      ; -----------------------------------------------------------------
  .ENDS
