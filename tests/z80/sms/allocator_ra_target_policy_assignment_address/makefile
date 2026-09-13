CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "register_allocator: assignment function=allocatorRaPolicyAssignment" compiler.log)" -eq 12
	test "$$(grep -c "target_specialized_address_materialization.*emitter=assignment_result_address" compiler.log)" -eq 12
	test "$$(grep -c "target_specialized_address_materialization.*emitter=assignment_arg1_address" compiler.log)" -eq 12
	grep -q "function=allocatorRaPolicyAssignmentGlobalByte emitter=assignment_result_address operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentGlobalByte emitter=assignment_arg1_address operand=arg1 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentNearByte emitter=assignment_result_address operand=result kind=STACK_LOCAL offset=-6 mode=frame_displacement folded_offset=-6 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentNearByte emitter=assignment_arg1_address operand=arg1 kind=STACK_LOCAL offset=-4 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentFarByte emitter=assignment_result_address operand=result kind=STACK_LOCAL offset=-147 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentFarByte emitter=assignment_arg1_address operand=arg1 kind=STACK_LOCAL offset=-146 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentGlobalWord emitter=assignment_result_address operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentGlobalWord emitter=assignment_arg1_address operand=arg1 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentNearWord emitter=assignment_result_address operand=result kind=STACK_LOCAL offset=-9 mode=frame_displacement folded_offset=-9 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentNearWord emitter=assignment_arg1_address operand=arg1 kind=STACK_LOCAL offset=-5 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentFarWord emitter=assignment_result_address operand=result kind=STACK_LOCAL offset=-151 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAssignmentFarWord emitter=assignment_arg1_address operand=arg1 kind=STACK_LOCAL offset=-149 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "assignment function=allocatorRaPolicyAssignmentSignExtend arg1_kind=STACK_LOCAL arg1_phy=NONE arg1_size=1 result_kind=GLOBAL result_phy=NONE result_size=2" compiler.log
	grep -q "assignment function=allocatorRaPolicyAssignmentZeroExtend arg1_kind=STACK_LOCAL arg1_phy=NONE arg1_size=1 result_kind=GLOBAL result_phy=NONE result_size=2" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*emitter=assignment_.*kind=PHY" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*emitter=assignment_.*mode=invalid" compiler.log
	: && ! grep -q "_load_location_address_to_i[xy]" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
