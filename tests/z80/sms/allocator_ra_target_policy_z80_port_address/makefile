CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "target_specialized_address_materialization.*emitter=z80_in_port_address" compiler.log)" -eq 3
	test "$$(grep -c "target_specialized_address_materialization.*emitter=z80_out_port_address" compiler.log)" -eq 3
	grep -q "function=allocatorRaPolicyZ80InGlobal emitter=z80_in_port_address operand=arg2 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyZ80OutGlobal emitter=z80_out_port_address operand=arg2 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyZ80InNear emitter=z80_in_port_address operand=arg2 kind=STACK_LOCAL offset=-4 mode=frame_displacement folded_offset=-4 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyZ80OutNear emitter=z80_out_port_address operand=arg2 kind=STACK_LOCAL offset=-4 mode=frame_displacement folded_offset=-4 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyZ80InFar emitter=z80_in_port_address operand=arg2 kind=STACK_LOCAL offset=-146 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyZ80OutFar emitter=z80_out_port_address operand=arg2 kind=STACK_LOCAL offset=-147 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "z80_in_port function=allocatorRaPolicyZ80InGlobal kind=GLOBAL size=1 phy=NONE" compiler.log
	grep -q "z80_out_port function=allocatorRaPolicyZ80OutNear kind=STACK_LOCAL size=1 phy=NONE" compiler.log
	grep -q "z80_in_port function=allocatorRaPolicyZ80InFar kind=STACK_LOCAL size=1 phy=NONE" compiler.log
	! grep -q "target_specialized_address_materialization.*emitter=z80_.*_port_address.*kind=PHY" compiler.log
	! grep -q "target_specialized_address_materialization.*emitter=z80_.*_port_address.*mode=invalid" compiler.log
	! grep -q "has no IX address materialization" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log