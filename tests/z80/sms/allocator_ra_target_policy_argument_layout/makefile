CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyCallByte target=z80 hooks=physical_register_units,active_slots,candidate_legality,tac_clobbers,tac_transparency,call_result_register,call_boundary_spill,stack_return_transport,stack_argument_layout,split_spill,split_reload.*argument_model=contiguous_stack" compiler.log
	grep -q "target_argument_layout function=allocatorRaPolicyCallByteCallee argument=1 size=1 target=z80 slot_start=-5 frame_end=-6" compiler.log
	grep -q "target_argument_layout function=allocatorRaPolicyCallWordCallee argument=1 size=2 target=z80 slot_start=-7 frame_end=-8" compiler.log
	grep -q "target_argument_layout function=allocatorRaPolicyCallByte argument=1 size=1 target=z80 slot_start=-5 frame_end=-6" compiler.log
	grep -q "target_argument_layout function=allocatorRaPolicyCallByte argument=2 size=1 target=z80 slot_start=-6 frame_end=-7" compiler.log
	grep -q "target_argument_layout function=allocatorRaPolicyCallWord argument=1 size=2 target=z80 slot_start=-7 frame_end=-8" compiler.log
	grep -q "target_argument_layout function=allocatorRaPolicyCallWord argument=2 size=2 target=z80 slot_start=-9 frame_end=-10" compiler.log
	grep -q "target_argument_layout function=allocatorRaPolicyCallAlternate argument=1 size=1 target=z80 slot_start=-5 frame_end=-6" compiler.log
	grep -q "target_argument_layout function=allocatorRaPolicyCallAlternate argument=2 size=1 target=z80 slot_start=-6 frame_end=-7" compiler.log
	grep -q "target_argument_layout function=allocatorRaPolicyCallAlternate argument=3 size=1 target=z80 slot_start=-7 frame_end=-8" compiler.log
	! grep -q "target_argument_layout function=main" compiler.log
	! grep -q "Target policy cannot place argument" compiler.log
	grep -q "copy argument 1" main.asm
	grep -q "copy argument 2" main.asm
	grep -q "copy argument 3" main.asm
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
