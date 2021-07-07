
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
      ; allocate space for the stack frame
      LD  HL,-6
      ADD HL,DE
      LD  SP,HL
      ; =================================================================
      ; ../test2.blb:4: uint8 color = 0;
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
      ; ../test2.blb:20: add_1(&color);
      ; =================================================================
      ; -----------------------------------------------------------------
      ; TAC: r0.uint16 (uint16) := &color.uint16 (uint16)
      ; -----------------------------------------------------------------
      ; offset -4
      LD  HL,-4
      ADD HL,DE
      ; offset -6
      LD  IX,-6
      ADD IX,DE
      LD  (IX+0),L
      LD  (IX+1),H
      ; -----------------------------------------------------------------
      ; TAC: add_1(r0.uint16 (uint16))
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
      LD  C,(IY-6)
      LD  B,(IY-5)
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
      ; TAC: return
      ; -----------------------------------------------------------------
      ; bytes -1 and 0 of stack frame contain the return address
      LD  HL,-1
      ADD HL,DE
      LD  SP,HL
      RET
  .ENDS

  .SECTION "add_1" FREE
    ; =================================================================
    ; ../test2.blb:23: void add_1(uint8 *color) {
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
      ; ../test2.blb:29: color[0] = color[0] + 1;
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
      ; offset -8
      LD  IX,-8
      ADD IX,DE
      ; offset -6
      LD  IY,-6
      ADD IY,DE
      LD  C,(IY+0)
      LD  B,0
      ; offset -5
      LD  IY,-5
      ADD IY,DE
      ADD IY,BC
      ADD IY,BC
      LD  L,(IX+0)
      LD  H,0
      LD  (IY+0),L
      LD  (IY+1),H
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
      ; TAC: _label_1:
      ; -----------------------------------------------------------------
    _label_1:
      ; -----------------------------------------------------------------
      ; TAC: jmp _label_1
      ; -----------------------------------------------------------------
      JP  _label_1
  .ENDS

