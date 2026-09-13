CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "target_address_materialization target=IY_OLD_FRAME" compiler.log)" -eq 8
	test "$$(grep -c "target_call_old_frame_address" compiler.log)" -eq 8
	grep -q "function=allocatorRaPolicyCallOldFrameAddress callee=allocatorRaPolicyCallOldFrameSink argument=1 kind=STACK_LOCAL offset=-4 mode=frame_displacement folded_offset=-4 cache_before=reset cache_action=initialize cache_after=ready address_target=IY_OLD_FRAME target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallOldFrameAddress callee=allocatorRaPolicyCallOldFrameSink argument=2 kind=STACK_LOCAL offset=-5 mode=frame_displacement folded_offset=-5 cache_before=ready cache_action=reuse cache_after=ready address_target=IY_OLD_FRAME target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallOldFrameAddress callee=allocatorRaPolicyCallOldFrameSink argument=3 kind=STACK_LOCAL offset=-127 mode=frame_address folded_offset=0 cache_before=ready cache_action=rebase cache_after=reset address_target=IY_OLD_FRAME target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallOldFrameAddress callee=allocatorRaPolicyCallOldFrameSink argument=4 kind=STACK_LOCAL offset=-7 mode=frame_displacement folded_offset=-7 cache_before=reset cache_action=initialize cache_after=ready address_target=IY_OLD_FRAME target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallOldFrameSpills callee=allocatorRaPolicyCallOldFrameSink argument=1 kind=STACK_SPILL offset=-8 mode=frame_displacement folded_offset=-8 cache_before=reset cache_action=initialize cache_after=ready address_target=IY_OLD_FRAME target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallOldFrameSpills callee=allocatorRaPolicyCallOldFrameSink argument=2 kind=STACK_SPILL offset=-9 mode=frame_displacement folded_offset=-9 cache_before=ready cache_action=reuse cache_after=ready address_target=IY_OLD_FRAME target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallOldFrameSpills callee=allocatorRaPolicyCallOldFrameSink argument=3 kind=STACK_SPILL offset=-10 mode=frame_displacement folded_offset=-10 cache_before=ready cache_action=reuse cache_after=ready address_target=IY_OLD_FRAME target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallOldFrameSpills callee=allocatorRaPolicyCallOldFrameSink argument=4 kind=STACK_SPILL offset=-11 mode=frame_displacement folded_offset=-11 cache_before=ready cache_action=reuse cache_after=ready address_target=IY_OLD_FRAME target=z80" compiler.log
	: && ! grep -q "target_address_materialization target=IY_OLD_FRAME.*mode=invalid" compiler.log
	: && ! grep -q "_load_call_argument_address_to_iy" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log