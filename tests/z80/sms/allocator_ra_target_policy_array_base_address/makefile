CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "target_specialized_address_materialization.*emitter=array_read_base_address" compiler.log)" -eq 4
	test "$$(grep -c "target_specialized_address_materialization.*emitter=array_write_base_address" compiler.log)" -eq 3
	grep -q "function=allocatorRaPolicyArrayBaseGlobalRead emitter=array_read_base_address operand=arg1 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyArrayBaseGlobalWrite emitter=array_write_base_address operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyArrayBaseNearRead emitter=array_read_base_address operand=arg1 kind=STACK_LOCAL offset=-5 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyArrayBaseNearWrite emitter=array_write_base_address operand=result kind=STACK_LOCAL offset=-5 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyArrayBaseFarRead emitter=array_read_base_address operand=arg1 kind=STACK_LOCAL offset=-148 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyArrayBaseFarWrite emitter=array_write_base_address operand=result kind=STACK_LOCAL offset=-151 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyArrayBaseFarWrite emitter=array_read_base_address operand=arg1 kind=STACK_LOCAL offset=-151 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "array_read_base function=allocatorRaPolicyArrayBaseGlobalRead kind=GLOBAL memory_pointer_load=yes" compiler.log
	grep -q "array_write_base function=allocatorRaPolicyArrayBaseFarWrite kind=STACK_LOCAL memory_pointer_load=no offset=-151" compiler.log
	grep -q "array_read_base function=allocatorRaPolicyArrayBaseRetainedRead kind=PHY_HL memory_pointer_load=no.*phy=HL" compiler.log
	grep -q "array_write_base function=allocatorRaPolicyArrayBaseRetainedWrite kind=PHY_HL memory_pointer_load=no.*phy=HL" compiler.log
	! grep -q "function=allocatorRaPolicyArrayBaseRetainedRead emitter=array_read_base_address" compiler.log
	! grep -q "function=allocatorRaPolicyArrayBaseRetainedWrite emitter=array_write_base_address" compiler.log
	! grep -q "target_specialized_address_materialization.*emitter=array_.*_base_address.*mode=invalid" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log