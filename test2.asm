  .SECTION "main" FREE
    main:
      // variable var size 2 offset 0 type n
      // var.w := g_i.b
      // offset 0
      ld h, d
      ld l, e
      ld bc, 0
      add hl, bc
      // return
  .ENDS
