
  .SECTION "main" FREE
    ; =================================================================
    ; ../test2.blb:2: void main(void) {
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
      ; ../test2.blb:4: int8 result = sum(1, 2);
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: variable "result" size 1 offset -4 type n
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
      ; TAC: result.int8 (int8) := sum(r0.uint8 (uint8), r1.uint8 (uint8))
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
      LD  (IX-5),L
      LD  L,(IY-6)
      LD  (IX-6),L
      LD  IX,-8
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
      LD  E,(IX-2)
      LD  D,(IX-3)
      ; offset -4
      LD  IY,-4
      ADD IY,DE
      LD  (IY+0),L
      ; =================================================================
      ; ../test2.blb:5: result++;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: result.int8 (int8) := result.int8 (int8) + 1.uint8 (int8)
      ; -----------------------------------------------------------------
      ; offset -4
      LD  IX,-4
      ADD IX,DE
      LD  A,(IX+0)
      INC A
      ; offset -4
      LD  IX,-4
      ADD IX,DE
      LD  (IX+0),A
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
    ; ../test2.blb:8: int8 sum(int8 a, int8 b) {
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
      ; TAC: variable "a" size 1 offset -5 type a
      ; -----------------------------------------------------------------
      ; -----------------------------------------------------------------
      ; TAC: variable "b" size 1 offset -6 type a
      ; -----------------------------------------------------------------
      ; =================================================================
      ; ../test2.blb:10: int8 c;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: variable "c" size 1 offset -7 type n
      ; -----------------------------------------------------------------
      ; =================================================================
      ; ../test2.blb:12: __asm("LD A,(@a)",
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: __asm("LD  A,(@a)",
      ;            "LD  B,(@b)",
      ;            "ADD A,B",
      ;            "LD  (@c),A");
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      LD  A,(IX+0)
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      LD  B,(IX+0)
      ADD A,B
      ; offset -7
      LD  IX,-7
      ADD IX,DE
      LD  (IX+0),A
      ; =================================================================
      ; ../test2.blb:17: return c;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: return c.int8 (int8)
      ; -----------------------------------------------------------------
      ; offset -7
      LD  IX,-7
      ADD IX,DE
      LD  L,(IX+0)
      LD  IX,0
      ADD IX,DE
      LD  (IX-4),L
      LD  L,(IX+0)
      LD  H,(IX-1)
      JP  (HL)
  .ENDS

  .SECTION "mainmain" FREE
    ; =================================================================
    ; INTERNAL
    ; =================================================================
    mainmain:
      ; A  - tmp
      ; BC - tmp
      ; DE - frame pointer
      ; HL - tmp
      ; SP - stack pointer
      ; IX - tmp
      ; IY - tmp
      ; -----------------------------------------------------------------
      ; TAC: main()
      ; -----------------------------------------------------------------
      LD  B,D
      LD  C,E
      LD  HL,0
      ADD HL,SP
      LD  D,H
      LD  E,L
      LD  IX,0
      ADD IX,DE
      LD  HL,_return_2
      LD  (IX+0),L
      LD  (IX-1),H
      LD  (IX-2),C
      LD  (IX-3),B
      LD  IX,-7
      ADD IX,DE
      LD  SP,IX
      JP  main
    _return_2:
      LD  H,D
      LD  L,E
      LD  SP,HL
      LD  IX,0
      ADD IX,DE
      LD  E,(IX-2)
      LD  D,(IX-3)
      ; -----------------------------------------------------------------
      ; TAC: _label_1:
      ; -----------------------------------------------------------------
    _label_1:
      ; -----------------------------------------------------------------
      ; TAC: jmp _label_1
      ; -----------------------------------------------------------------
      JP  _label_1
  .ENDS

