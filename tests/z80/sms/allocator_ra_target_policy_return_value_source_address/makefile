CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "return_value_source function=allocatorRaPolicyReturnSource" compiler.log)" -eq 22
	test "$$(grep -c "target_specialized_address_materialization.*emitter=return_value_source_address" compiler.log)" -eq 8
	grep -q "return_value_source function=allocatorRaPolicyReturnSourceGlobalByte kind=GLOBAL size=1 phy=NONE arg_type=4 return_type=4" compiler.log
	grep -q "return_value_source function=allocatorRaPolicyReturnSourceGlobalWord kind=GLOBAL size=2 phy=NONE arg_type=5 return_type=5" compiler.log
	grep -q "function=allocatorRaPolicyReturnSourceGlobalByte emitter=return_value_source_address operand=arg1 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyReturnSourceGlobalWord emitter=return_value_source_address operand=arg1 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyReturnSourceNearByte emitter=return_value_source_address operand=arg1 kind=STACK_LOCAL offset=-5 mode=frame_displacement folded_offset=-5 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyReturnSourceNearWord emitter=return_value_source_address operand=arg1 kind=STACK_LOCAL offset=-7 mode=frame_displacement folded_offset=-7 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyReturnSourceFarByte emitter=return_value_source_address operand=arg1 kind=STACK_LOCAL offset=-146 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyReturnSourceFarWord emitter=return_value_source_address operand=arg1 kind=STACK_LOCAL offset=-149 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "return_value_source function=allocatorRaPolicyReturnSourceSignedWiden kind=STACK_LOCAL size=1 phy=NONE arg_type=2 return_type=3" compiler.log
	grep -q "return_value_source function=allocatorRaPolicyReturnSourceUnsignedWiden kind=STACK_LOCAL size=1 phy=NONE arg_type=4 return_type=5" compiler.log
	grep -q "function=allocatorRaPolicyReturnSourceSignedWiden emitter=return_value_source_address operand=arg1 kind=STACK_LOCAL offset=-6 mode=frame_displacement folded_offset=-6 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyReturnSourceUnsignedWiden emitter=return_value_source_address operand=arg1 kind=STACK_LOCAL offset=-6 mode=frame_displacement folded_offset=-6 address_target=IX target=z80" compiler.log
	test "$$(grep -c "return_boundary_consumer function=allocatorRaPolicyReturnSourceRetained" compiler.log)" -eq 2
	grep -q "linear_scan retain function=allocatorRaPolicyReturnSourceRetainedA.*8-bit in A.*consumer_tac=.*RETURN_VALUE arg1" compiler.log
	grep -q "linear_scan retain function=allocatorRaPolicyReturnSourceRetainedHl.*16-bit in HL.*consumer_tac=.*RETURN_VALUE arg1" compiler.log
	grep -q "return_value_source function=allocatorRaPolicyReturnSourceRetainedA kind=PHY_A size=1 phy=A arg_type=4 return_type=4" compiler.log
	grep -q "return_value_source function=allocatorRaPolicyReturnSourceRetainedHl kind=PHY_HL size=2 phy=HL arg_type=5 return_type=5" compiler.log
	grep -q "return_value_source function=allocatorRaPolicyReturnSourceConstant kind=CONST size=1 phy=NONE arg_type=4 return_type=4" compiler.log
	! grep -q "function=allocatorRaPolicyReturnSourceRetained.*emitter=return_value_source_address" compiler.log
	! grep -q "function=allocatorRaPolicyReturnSourceConstant.*emitter=return_value_source_address" compiler.log
	! grep -q "target_specialized_address_materialization.*emitter=return_value_source_address.*kind=PHY" compiler.log
	! grep -q "target_specialized_address_materialization.*emitter=return_value_source_address.*mode=invalid" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log