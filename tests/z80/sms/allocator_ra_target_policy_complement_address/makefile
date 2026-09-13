CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "target_specialized_address_materialization.*emitter=complement_8bit_arg1_address" compiler.log)" -eq 4
	test "$$(grep -c "target_specialized_address_materialization.*emitter=complement_8bit_result_address" compiler.log)" -eq 3
	test "$$(grep -c "target_specialized_address_materialization.*emitter=complement_16bit_arg1_address" compiler.log)" -eq 4
	test "$$(grep -c "target_specialized_address_materialization.*emitter=complement_16bit_result_address" compiler.log)" -eq 3
	grep -q "function=allocatorRaPolicyComplementGlobalByte emitter=complement_8bit_arg1_address operand=arg1 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementGlobalByte emitter=complement_8bit_result_address operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementNearByte emitter=complement_8bit_arg1_address operand=arg1 kind=STACK_LOCAL offset=-4 mode=frame_displacement folded_offset=-4 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementNearByte emitter=complement_8bit_result_address operand=result kind=STACK_SPILL offset=-5 mode=frame_displacement folded_offset=-5 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementFarByte emitter=complement_8bit_arg1_address operand=arg1 kind=STACK_LOCAL offset=-146 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementFarByte emitter=complement_8bit_result_address operand=result kind=STACK_SPILL offset=-147 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementGlobalWord emitter=complement_16bit_arg1_address operand=arg1 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementGlobalWord emitter=complement_16bit_result_address operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementNearWord emitter=complement_16bit_arg1_address operand=arg1 kind=STACK_LOCAL offset=-5 mode=frame_displacement folded_offset=-5 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementNearWord emitter=complement_16bit_result_address operand=result kind=STACK_SPILL offset=-7 mode=frame_displacement folded_offset=-7 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementFarWord emitter=complement_16bit_arg1_address operand=arg1 kind=STACK_LOCAL offset=-149 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyComplementFarWord emitter=complement_16bit_result_address operand=result kind=STACK_SPILL offset=-151 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "complement_8bit function=allocatorRaPolicyComplementRetainedResultByte.*result_kind=PHY_A result_phy=A" compiler.log
	grep -q "function=allocatorRaPolicyComplementRetainedResultByte emitter=complement_8bit_arg1_address" compiler.log
	: && ! grep -q "function=allocatorRaPolicyComplementRetainedResultByte emitter=complement_8bit_result_address" compiler.log
	grep -q "complement_16bit function=allocatorRaPolicyComplementRetainedResultWord.*result_kind=PHY_HL result_phy=HL" compiler.log
	grep -q "function=allocatorRaPolicyComplementRetainedResultWord emitter=complement_16bit_arg1_address" compiler.log
	: && ! grep -q "function=allocatorRaPolicyComplementRetainedResultWord emitter=complement_16bit_result_address" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*emitter=complement_.*kind=PHY" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*emitter=complement_.*mode=invalid" compiler.log
	: && ! grep -q "_load_location_address_to_ix" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log