
  .RAMSECTION "global_variables_test2_ram" FREE
    g_i     DSB 8
    g_value DW
  .ENDS

  .SECTION "global_variables_test2_rom" FREE
    global_variables_test2_rom:
      ; g_i
      .DB 0, 0, 0, 0, 0, 0, 0, 0
      ; g_value
      .DW 1
  .ENDS

  .SECTION "global_variables_test2_init" FREE
    global_variables_test2_init:
      LD  HL,g_i
      LD  DE,global_variables_test2_rom
      LD  B,10
    -
      LD  A,(DE)
      LD  (HL),A
      INC DE
      INC HL
      DJNZ -
  .ENDS

  .SECTION "main" FREE
    ; =================================================================
    ; ../test2.blb:5: void main() {
    ; =================================================================
    main:
      ; A  - tmp
      ; BC - tmp
      ; DE - frame pointer
      ; HL - tmp
      ; SP - stack pointer
      ; IX - tmp
      ; IY - tmp
      ; =================================================================
      ; ../test2.blb:7: int8 var = sum(1, 2);
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: variable "var" size 1 offset -4 type n
      ; -----------------------------------------------------------------
      ; -----------------------------------------------------------------
      ; TAC: r0.uint8 (uint8) := 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      LD  (IX+0),1
      ; -----------------------------------------------------------------
      ; TAC: r1.uint8 (uint8) := 2.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      LD  (IX+0),2
      ; -----------------------------------------------------------------
      ; TAC: var.int8 (int8) := sum(r0.uint8 (uint8), r1.uint8 (uint8))
      ; -----------------------------------------------------------------
      LD  B,D
      LD  C,E
      LD  HL,0
      ADD HL,SP
      LD  D,H
      LD  E,L
      LD  IX,0
      ADD IX,DE
      LD  HL,_return_1
      LD  (IX+0),L
      LD  (IX-1),H
      LD  (IX-2),C
      LD  (IX-3),B
      LD  IY,0
      ADD IY,BC
      LD  L,(IY-5)
      LD  (IX-6),L
      LD  L,(IY-6)
      LD  (IX-7),L
      LD  IX,-9
      ADD IX,DE
      LD  SP,IX
      JP  sum
    _return_1:
      LD  H,D
      LD  L,E
      LD  SP,HL
      LD  IX,0
      ADD IX,DE
      LD  L,(IX-4)
      LD  H,(IX-5)
      LD  E,(IX-2)
      LD  D,(IX-3)
      ; offset -4
      LD  IY,-4
      ADD IY,DE
      LD  (IY+0),L
      ; -----------------------------------------------------------------
      ; TAC: return
      ; -----------------------------------------------------------------
      LD  IX,0
      ADD IX,DE
      LD  L,(IX+0)
      LD  H,(IX-1)
      JP  (HL)
  .ENDS

  .SECTION "sum" FREE
    ; =================================================================
    ; ../test2.blb:10: int16 sum(int8 a, int8 b) {
    ; =================================================================
    sum:
      ; A  - tmp
      ; BC - tmp
      ; DE - frame pointer
      ; HL - tmp
      ; SP - stack pointer
      ; IX - tmp
      ; IY - tmp
      ; -----------------------------------------------------------------
      ; TAC: variable "a" size 1 offset -6 type a
      ; -----------------------------------------------------------------
      ; -----------------------------------------------------------------
      ; TAC: variable "b" size 1 offset -7 type a
      ; -----------------------------------------------------------------
      ; =================================================================
      ; ../test2.blb:12: return a + b;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: r0.int8 (int8) := a.int8 (int8) + b.int8 (int8)
      ; -----------------------------------------------------------------
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      LD  A,(IX+0)
      ; offset -7
      LD  IX,-7
      ADD IX,DE
      ADD A,(IX+0)
      ; offset -8
      LD  IX,-8
      ADD IX,DE
      LD  (IX+0),A
      ; -----------------------------------------------------------------
      ; TAC: return r0.int8 (int8)
      ; -----------------------------------------------------------------
      ; offset -8
      LD  IX,-8
      ADD IX,DE
      LD  L,(IX+0)
      ; sign extend 8-bit L -> 16-bit HL
      LD  A,L
      ADD A,A  ; sign bit of A into carry
      SBC A,A  ; A = 0 if carry == 0, $FF otherwise
      LD  H,A  ; now HL is sign extended L
      LD  IX,0
      ADD IX,DE
      LD  (IX-4),L
      LD  (IX-5),H
      LD  L,(IX+0)
      LD  H,(IX-1)
      JP  (HL)
  .ENDS

