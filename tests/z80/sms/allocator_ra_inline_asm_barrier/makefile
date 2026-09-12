CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "inline_asm_contract function=allocatorRaInlineAsmBarrierA.*policy=hard_barrier.*operand_policy=stack_location.*clobbers=A|BC|DE|HL|IX|IY|SP|FLAGS" compiler.log
	grep -q "inline_asm_contract function=allocatorRaInlineAsmBarrierHl.*policy=hard_barrier.*operand_policy=stack_location.*clobbers=A|BC|DE|HL|IX|IY|SP|FLAGS" compiler.log
	grep -q "inline_asm_operand function=allocatorRaInlineAsmBarrierA access=read variable=.*scratch.*kind=STACK_LOCAL" compiler.log
	grep -q "inline_asm_operand function=allocatorRaInlineAsmBarrierA access=write variable=.*scratch.*kind=STACK_LOCAL" compiler.log
	grep -q "inline_asm_operand function=allocatorRaInlineAsmBarrierHl access=read variable=.*scratch.*kind=STACK_LOCAL" compiler.log
	grep -q "inline_asm_operand function=allocatorRaInlineAsmBarrierHl access=write variable=.*scratch.*kind=STACK_LOCAL" compiler.log
	grep -q "function=allocatorRaInlineAsmBarrierA emitter=inline_asm_read_address operand=variable kind=STACK_LOCAL offset=-8 mode=frame_displacement folded_offset=-8 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaInlineAsmBarrierA emitter=inline_asm_write_address operand=variable kind=STACK_LOCAL offset=-8 mode=frame_displacement folded_offset=-8 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaInlineAsmBarrierHl emitter=inline_asm_read_address operand=variable kind=STACK_LOCAL offset=-13 mode=frame_displacement folded_offset=-13 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaInlineAsmBarrierHl emitter=inline_asm_write_address operand=variable kind=STACK_LOCAL offset=-13 mode=frame_displacement folded_offset=-13 address_target=IX target=z80" compiler.log
	grep -q "z80_clobber function=allocatorRaInlineAsmBarrierA.*(ASM).*clobbers=A|BC|DE|HL|IX|IY|SP|FLAGS" compiler.log
	grep -q "z80_clobber function=allocatorRaInlineAsmBarrierHl.*(ASM).*clobbers=A|BC|DE|HL|IX|IY|SP|FLAGS" compiler.log
	! grep -q "inline_asm_operand.*kind=PHY" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log