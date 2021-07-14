
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
      LD  IX,0
      ADD IX,DE
      LD  (IX-4),0
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
      ; ../test2.blb:14: for (i = 0; i < 32; i++) {
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: i.uint8 (uint8) := 0.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IX,0
      ADD IX,DE
      LD  (IX-5),0
      ; -----------------------------------------------------------------
      ; TAC: _label_3:
      ; -----------------------------------------------------------------
    _label_3:
      ; -----------------------------------------------------------------
      ; TAC: r0.uint8 (uint8) := 0.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -6
      LD  IX,0
      ADD IX,DE
      LD  (IX-6),0
      ; -----------------------------------------------------------------
      ; TAC: if i.uint8 (uint8) < 32.uint8 (uint8) jmp _label_6
      ; -----------------------------------------------------------------
      ; offset -5
      LD  A,(IX-5)
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
      LD  IX,0
      ADD IX,DE
      LD  (IX-6),1
      ; -----------------------------------------------------------------
      ; TAC: _label_7:
      ; -----------------------------------------------------------------
    _label_7:
      ; -----------------------------------------------------------------
      ; TAC: if r0.uint8 (uint8) == 0.uint8 (uint8) jmp _label_5
      ; -----------------------------------------------------------------
      ; offset -6
      LD  IX,0
      ADD IX,DE
      LD  A,(IX-6)
      LD  B,0
      SUB A,B
      JP  Z,_label_5
      ; =================================================================
      ; ../test2.blb:15: __z80_out[0xBE] = color;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: __z80_out.uint8 (uint8)[190.uint8 (uint8)] := color.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -4
      LD  A,(IX-4)
      OUT ($BE),A
      ; -----------------------------------------------------------------
      ; TAC: _label_4:
      ; -----------------------------------------------------------------
    _label_4:
      ; -----------------------------------------------------------------
      ; TAC: i.uint8 (uint8) := i.uint8 (uint8) + 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IX,0
      ADD IX,DE
      LD  A,(IX-5)
      INC A
      ; offset -5
      LD  (IX-5),A
      ; -----------------------------------------------------------------
      ; TAC: jmp _label_3
      ; -----------------------------------------------------------------
      JP  _label_3
      ; -----------------------------------------------------------------
      ; TAC: _label_5:
      ; -----------------------------------------------------------------
    _label_5:
      ; =================================================================
      ; ../test2.blb:18: add_to_color(&color, 1);
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: r1.uint16 (uint16) := &color.uint16 (uint16)
      ; -----------------------------------------------------------------
      ; offset -4
      LD  HL,-4
      ADD HL,DE
      ; offset -8
      LD  IX,0
      ADD IX,DE
      LD  (IX-8),L
      LD  (IX-7),H
      ; -----------------------------------------------------------------
      ; TAC: add_to_color(r1.uint16, 1.uint8 (uint8))
      ; -----------------------------------------------------------------
      LD  HL,0
      ADD HL,SP
      DEC HL
      LD  BC,_return_1
      PUSH BC
      PUSH DE
      ; new stack frame -> IX
      LD  IX,0
      LD  B,H
      LD  C,L
      ADD IX,BC
      ; old stack frame -> IY
      LD  IY,0
      ADD IY,DE
      ; copy argument 1
      LD  C,(IY-8)
      LD  B,(IY-7)
      LD  (IX-5),C
      LD  (IX-4),B
      ; copy argument 2
      LD  (IX-6),1
      ; new stack frame -> DE
      LD  D,H
      LD  E,L
      JP  add_to_color
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

  .SECTION "add_to_color" FREE
    ; =================================================================
    ; ../test2.blb:22: void add_to_color(uint8 *color, uint8 adder) {
    ; =================================================================
    add_to_color:
      ; A  - tmp
      ; BC - tmp
      ; DE - frame pointer
      ; HL - tmp
      ; SP - stack pointer
      ; IX - tmp
      ; IY - tmp
      ; allocate space for the stack frame
      LD  HL,-14
      ADD HL,DE
      LD  SP,HL
      ; -----------------------------------------------------------------
      ; TAC: variable "color" size 2 offset -5 type a
      ; -----------------------------------------------------------------
      ; -----------------------------------------------------------------
      ; TAC: variable "adder" size 1 offset -6 type a
      ; -----------------------------------------------------------------
      ; =================================================================
      ; ../test2.blb:24: color[0] += 1+adder-1+1-1+1-1;
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: r0.uint8 (uint8) := color.uint16 (uint8)[0.uint8 (uint8)]
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IY,0
      ADD IY,DE
      LD  C,(IY-5)
      LD  B,(IY-4)
      LD  IY,0
      ADD IY,BC
      LD  L,(IY+0)
      ; offset -7
      LD  IX,0
      ADD IX,DE
      LD  (IX-7),L
      ; -----------------------------------------------------------------
      ; TAC: r1.uint8 (uint8) := adder.uint8 (uint8) + 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -6
      LD  A,(IX-6)
      INC A
      ; offset -8
      LD  (IX-8),A
      ; -----------------------------------------------------------------
      ; TAC: r2.uint8 (uint8) := r1.uint8 (uint8) - 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -8
      LD  A,(IX-8)
      DEC A
      ; offset -9
      LD  (IX-9),A
      ; -----------------------------------------------------------------
      ; TAC: r3.uint8 (uint8) := r2.uint8 (uint8) + 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -9
      LD  A,(IX-9)
      INC A
      ; offset -10
      LD  (IX-10),A
      ; -----------------------------------------------------------------
      ; TAC: r4.uint8 (uint8) := r3.uint8 (uint8) - 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -10
      LD  A,(IX-10)
      DEC A
      ; offset -11
      LD  (IX-11),A
      ; -----------------------------------------------------------------
      ; TAC: r5.uint8 (uint8) := r4.uint8 (uint8) + 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -11
      LD  A,(IX-11)
      INC A
      ; offset -12
      LD  (IX-12),A
      ; -----------------------------------------------------------------
      ; TAC: r6.uint8 (uint8) := r5.uint8 (uint8) - 1.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -12
      LD  A,(IX-12)
      DEC A
      ; offset -13
      LD  (IX-13),A
      ; -----------------------------------------------------------------
      ; TAC: r7.uint8 (uint8) := r0.uint8 (uint8) + r6.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -7
      LD  A,(IX-7)
      ; offset -13
      ADD A,(IX-13)
      ; offset -14
      LD  (IX-14),A
      ; -----------------------------------------------------------------
      ; TAC: color.uint16 (uint8)[0.uint8 (uint8)] := r7.uint8 (uint8)
      ; -----------------------------------------------------------------
      ; offset -5
      LD  IY,0
      ADD IY,DE
      LD  C,(IY-5)
      LD  B,(IY-4)
      LD  IY,0
      ADD IY,BC
      ; offset -14
      LD  L,(IX-14)
      LD  (IY+0),L
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

