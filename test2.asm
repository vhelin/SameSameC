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
      ; test2.blb:7: int16 a = g_value * 1111;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: variable "a" size 2 offset 0 type n
      ; -----------------------------------------------------------------
      ; -----------------------------------------------------------------
      ; TAC: a.int16 (int16) := g_value.int8 (int16) * 1111.int16 (int16)
      ; -----------------------------------------------------------------
      LD  IX,g_value
      LD  C,(IX+0)
      ; sign extend 8-bit C -> 16-bit BC
      LD  A,C
      ADD A,A  ; sign bit of A into carry
      SBC A,A  ; A = 0 if carry == 0, $FF otherwise
      LD  B,A  ; now BC is sign extended C
      PUSH DE
      LD  DE,1111
      ; multiply BC * DE -> HL
      LD  A,16 ; number of bits to process
      LD  HL,0
    -
      SRL B
      RR  C
      JR  NC, +
      ADD HL,DE
    +
      EX  DE,HL
      ADD HL,HL
      EX  DE,HL
      DEC A
      JR  NZ,-
      POP DE
      ; offset 0
      LD  IX,0
      ADD IX,DE
      LD  (IX+0),L
      LD  (IX+1),H
      ; -----------------------------------------------------------------
      ; TAC: return
      ; -----------------------------------------------------------------
  .ENDS
