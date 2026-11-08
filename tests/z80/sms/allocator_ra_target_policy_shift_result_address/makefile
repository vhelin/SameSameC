CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "register_allocator: shift_result function=allocatorRaPolicyShift" compiler.log)" -eq 8
	test "$$(grep -c "target_specialized_address_materialization.*emitter=shift_8bit_result_address" compiler.log)" -eq 0
	test "$$(grep -c "target_specialized_address_materialization.*emitter=shift_16bit_result_address" compiler.log)" -eq 8
	grep -q "shift_result function=allocatorRaPolicyShiftGlobalByteLeft op=SHIFT_LEFT kind=GLOBAL size=1 phy=NONE" compiler.log
	grep -q "shift_result function=allocatorRaPolicyShiftGlobalByteRight op=SHIFT_RIGHT kind=GLOBAL size=1 phy=NONE" compiler.log
	grep -q "function=allocatorRaPolicyShiftGlobalByteLeft emitter=shift_16bit_result_address operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyShiftGlobalByteRight emitter=shift_16bit_result_address operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyShiftNearByteLeft emitter=shift_16bit_result_address operand=result kind=STACK_LOCAL offset=-7 mode=frame_displacement folded_offset=-7 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyShiftFarByteRight emitter=shift_16bit_result_address operand=result kind=STACK_LOCAL offset=-147 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "shift_result function=allocatorRaPolicyShiftGlobalWordLeft op=SHIFT_LEFT kind=GLOBAL size=2 phy=NONE" compiler.log
	grep -q "shift_result function=allocatorRaPolicyShiftGlobalWordRight op=SHIFT_RIGHT kind=GLOBAL size=2 phy=NONE" compiler.log
	grep -q "function=allocatorRaPolicyShiftGlobalWordLeft emitter=shift_16bit_result_address operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyShiftGlobalWordRight emitter=shift_16bit_result_address operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyShiftNearWordLeft emitter=shift_16bit_result_address operand=result kind=STACK_LOCAL offset=-11 mode=frame_displacement folded_offset=-11 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyShiftFarWordRight emitter=shift_16bit_result_address operand=result kind=STACK_LOCAL offset=-151 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*emitter=shift_.*result_address.*kind=PHY" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*emitter=shift_.*result_address.*mode=invalid" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
