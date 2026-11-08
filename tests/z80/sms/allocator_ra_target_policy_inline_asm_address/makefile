CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "target_specialized_address_materialization.*emitter=inline_asm_read_address" compiler.log)" -eq 6
	test "$$(grep -c "target_specialized_address_materialization.*emitter=inline_asm_write_address" compiler.log)" -eq 6
	grep -q "function=allocatorRaPolicyInlineAsmGlobalByte emitter=inline_asm_read_address operand=variable kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyInlineAsmGlobalWord emitter=inline_asm_write_address operand=variable kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyInlineAsmNearByte emitter=inline_asm_read_address operand=variable kind=STACK_LOCAL offset=-6 mode=frame_displacement folded_offset=-6 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyInlineAsmNearByte emitter=inline_asm_write_address operand=variable kind=STACK_LOCAL offset=-6 mode=frame_displacement folded_offset=-6 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyInlineAsmNearWord emitter=inline_asm_read_address operand=variable kind=STACK_LOCAL offset=-9 mode=frame_displacement folded_offset=-9 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyInlineAsmNearWord emitter=inline_asm_write_address operand=variable kind=STACK_LOCAL offset=-9 mode=frame_displacement folded_offset=-9 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyInlineAsmFarByte emitter=inline_asm_read_address operand=variable kind=STACK_LOCAL offset=-146 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyInlineAsmFarByte emitter=inline_asm_write_address operand=variable kind=STACK_LOCAL offset=-146 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyInlineAsmFarWord emitter=inline_asm_read_address operand=variable kind=STACK_LOCAL offset=-149 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyInlineAsmFarWord emitter=inline_asm_write_address operand=variable kind=STACK_LOCAL offset=-149 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "inline_asm_operand function=allocatorRaPolicyInlineAsmGlobalByte access=read.*kind=GLOBAL.*size=1 policy=stack_location" compiler.log
	grep -q "inline_asm_operand function=allocatorRaPolicyInlineAsmGlobalWord access=write.*kind=GLOBAL.*size=2 policy=stack_location" compiler.log
	grep -q "inline_asm_operand function=allocatorRaPolicyInlineAsmFarByte access=read.*kind=STACK_LOCAL offset=-146 size=1 policy=stack_location" compiler.log
	grep -q "inline_asm_operand function=allocatorRaPolicyInlineAsmFarWord access=write.*kind=STACK_LOCAL offset=-149 size=2 policy=stack_location" compiler.log
	grep -q "inline_asm_contract function=allocatorRaPolicyInlineAsmFarByte.*policy=hard_barrier operand_policy=stack_location clobbers=A|BC|DE|HL|IX|IY|SP|FLAGS" compiler.log
	: && ! grep -q "inline_asm_operand.*kind=PHY" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*emitter=inline_asm_.*mode=invalid" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log