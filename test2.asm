
  .SECTION "main" FREE
    ; =================================================================
    ; ../test2.blb:5: void main(void) {
    ; =================================================================
    main:
      ; A  - tmp
      ; BC - tmp
      ; DE - frame pointer
      ; HL - tmp
      ; SP - stack pointer
      ; IX - tmp
      ; IY - tmp
      ; allocate space for the stack frame
      LD  HL,-8
      ADD HL,DE
      LD  SP,HL
      ; =================================================================
      ; ../test2.blb:7: uint8 color = 0;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: variable "color" size 1 offset -4 type n
      ; -----------------------------------------------------------------
      ; -----------------------------------------------------------------
      ; TAC: color.uint8 (uint8) := 0.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -4
      LD  IX,-4
      ADD IX,DE
      LD  (IX+0),0
      ; =================================================================
      ; ../test2.blb:8: uint8 i;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: variable "i" size 1 offset -5 type n
      ; -----------------------------------------------------------------
      ; =================================================================
      ; ../test2.blb:10: while (1) {
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: _label_1:
      ; -----------------------------------------------------------------
    _label_1:
      ; =================================================================
      ; ../test2.blb:11: __z80_out[0xBF] = 0x00;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: __z80_out.uint8 (uint8)[191.uint8 (uint8)] := 0.uint8 (uint8)
      ; -----------------------------------------------------------------
      LD  A,0
      OUT ($BF),A
      ; =================================================================
      ; ../test2.blb:12: __z80_out[0xBF] = 0xC0;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: __z80_out.uint8 (uint8)[191.uint8 (uint8)] := 192.uint8 (uint8)
      ; -----------------------------------------------------------------
      LD  A,192
      OUT ($BF),A
      ; =================================================================
      ; ../test2.blb:13: for (i = 0; i < 32; i++) {
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: i.uint8 (uint8) := 0.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      LD  (IX+0),0
      ; -----------------------------------------------------------------
      ; TAC: _label_3:
      ; -----------------------------------------------------------------
    _label_3:
      ; -----------------------------------------------------------------
      ; TAC: r0.uint8 (uint8) := 0.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      LD  (IX+0),0
      ; -----------------------------------------------------------------
      ; TAC: if i.uint8 (uint8) < 32.uint8 (uint8) jmp _label_6
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      LD  A,(IX+0)
      LD  B,32
      SUB A,B
      JP  C,_label_6
      ; -----------------------------------------------------------------
      ; TAC: jmp _label_7
      ; -----------------------------------------------------------------
      JP  _label_7
      ; -----------------------------------------------------------------
      ; TAC: _label_6:
      ; -----------------------------------------------------------------
    _label_6:
      ; -----------------------------------------------------------------
      ; TAC: r0.uint8 (uint8) := 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      LD  (IX+0),1
      ; -----------------------------------------------------------------
      ; TAC: _label_7:
      ; -----------------------------------------------------------------
    _label_7:
      ; -----------------------------------------------------------------
      ; TAC: if r0.uint8 (uint8) == 0.uint8 (uint8) jmp _label_5
      ; -----------------------------------------------------------------
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      LD  A,(IX+0)
      LD  B,0
      SUB A,B
      JP  Z,_label_5
      ; =================================================================
      ; ../test2.blb:14: __z80_out[0xBE] = color;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: __z80_out.uint8 (uint8)[190.uint8 (uint8)] := color.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -4
      LD  IX,-4
      ADD IX,DE
      LD  A,(IX+0)
      OUT ($BE),A
      ; -----------------------------------------------------------------
      ; TAC: _label_4:
      ; -----------------------------------------------------------------
    _label_4:
      ; -----------------------------------------------------------------
      ; TAC: i.uint8 (uint8) := i.uint8 (uint8) + 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      LD  A,(IX+0)
      INC A
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      LD  (IX+0),A
      ; -----------------------------------------------------------------
      ; TAC: jmp _label_3
      ; -----------------------------------------------------------------
      JP  _label_3
      ; -----------------------------------------------------------------
      ; TAC: _label_5:
      ; -----------------------------------------------------------------
    _label_5:
      ; =================================================================
      ; ../test2.blb:17: add_1(&color);
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: r1.uint16 (uint16) := &color.uint16 (uint16)
      ; -----------------------------------------------------------------
      ; offset -4
      LD  HL,-4
      ADD HL,DE
      ; offset -8
      LD  IX,-8
      ADD IX,DE
      LD  (IX+0),L
      LD  (IX+1),H
      ; -----------------------------------------------------------------
      ; TAC: add_1(r1.uint16 (uint16))
      ; -----------------------------------------------------------------
      LD  HL,0
      ADD HL,SP
      DEC HL
      LD  BC,_return_1
      PUSH BC
      PUSH DE
      ; new stack frame -> IX
      LD  B,H
      LD  C,L
      LD  IX,0
      ADD IX,BC
      ; old stack frame -> IY
      LD  IY,0
      ADD IY,DE
      ; copy argument 1
      LD  C,(IY-8)
      LD  B,(IY-7)
      LD  (IX-5),C
      LD  (IX-4),B
      ; new stack frame -> DE
      LD  D,H
      LD  E,L
      JP  add_1
    _return_1:
      ; new stack frame -> IX
      LD  IX,0
      ADD IX,DE
      ; old stack frame address -> DE
      LD  E,(IX-3)
      LD  D,(IX-2)
      ; -----------------------------------------------------------------
      ; TAC: jmp _label_1
      ; -----------------------------------------------------------------
      JP  _label_1
  .ENDS

  .SECTION "add_1" FREE
    ; =================================================================
    ; ../test2.blb:21: void add_1(uint8 *color) {
    ; =================================================================
    add_1:
      ; A  - tmp
      ; BC - tmp
      ; DE - frame pointer
      ; HL - tmp
      ; SP - stack pointer
      ; IX - tmp
      ; IY - tmp
      ; allocate space for the stack frame
      LD  HL,-8
      ADD HL,DE
      LD  SP,HL
      ; -----------------------------------------------------------------
      ; TAC: variable "color" size 2 offset -5 type a
      ; -----------------------------------------------------------------
      ; =================================================================
      ; ../test2.blb:23: color[2-1-1] += (ONE(2,-1)+2-1-1);
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: r0.uint8 (uint8) := 0.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      LD  (IX+0),0
      ; -----------------------------------------------------------------
      ; TAC: r1.uint8 (uint8) := color.uint16 (uint8)[0.uint8 (uint8)]
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      LD  C,(IX+0)
      LD  B,(IX+1)
      LD  IX,0
      ADD IX,BC
      LD  L,(IX+0)
      ; offset -7
      LD  IX,-7
      ADD IX,DE
      LD  (IX+0),L
      ; -----------------------------------------------------------------
      ; TAC: r2.uint8 (uint8) := r1.uint8 (uint8) + 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -7
      LD  IX,-7
      ADD IX,DE
      LD  A,(IX+0)
      INC A
      ; offset -8
      LD  IX,-8
      ADD IX,DE
      LD  (IX+0),A
      ; -----------------------------------------------------------------
      ; TAC: color.uint16 (uint8)[r0.uint8 (uint8)] := r2.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      LD  C,(IX+0)
      LD  B,0
      ; offset -5
      LD  IX,-5
      ADD IX,DE
      PUSH BC
      LD  C,(IX+0)
      LD  B,(IX+1)
      LD  IX,0
      ADD IX,BC
      POP BC
      ADD IX,BC
      ; offset -8
      LD  IY,-8
      ADD IY,DE
      LD  L,(IY+0)
      LD  (IX+0),L
      ; -----------------------------------------------------------------
      ; TAC: return
      ; -----------------------------------------------------------------
      ; bytes -1 and 0 of stack frame contain the return address
      LD  HL,-1
      ADD HL,DE
      LD  SP,HL
      RET
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
      LD  HL,0
      ADD HL,SP
      DEC HL
      LD  BC,_return_2
      PUSH BC
      PUSH DE
      ; new stack frame -> DE
      LD  D,H
      LD  E,L
      JP  main
    _return_2:
      ; new stack frame -> IX
      LD  IX,0
      ADD IX,DE
      ; old stack frame address -> DE
      LD  E,(IX-3)
      LD  D,(IX-2)
      ; -----------------------------------------------------------------
      ; TAC: _label_8:
      ; -----------------------------------------------------------------
    _label_8:
      ; -----------------------------------------------------------------
      ; TAC: jmp _label_8
      ; -----------------------------------------------------------------
      JP  _label_8
  .ENDS

