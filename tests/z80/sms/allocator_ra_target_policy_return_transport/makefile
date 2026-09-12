CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyCallByte target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*stack_return_model=frame_slot.*argument_model=contiguous_stack" compiler.log
	grep -q "target_return_transport function=allocatorRaPolicyCallByteCallee size=1 target=z80 slot_start=-4 frame_end=-5" compiler.log
	grep -q "target_return_transport function=allocatorRaPolicyCallWordCallee size=2 target=z80 slot_start=-5 frame_end=-6" compiler.log
	grep -q "target_return_transport function=main size=0 target=z80 slot_start=0 frame_end=-4" compiler.log
	grep -q 'OFFSET -00004 SIZE 000001 "return value"' compiler.log
	grep -q 'OFFSET -00005 SIZE 000002 "return value"' compiler.log
	! grep -A4 'STACK FRAME of FUNCTION "main"' compiler.log | grep -q '"return value"'
	! grep -q "Target policy cannot reserve" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log