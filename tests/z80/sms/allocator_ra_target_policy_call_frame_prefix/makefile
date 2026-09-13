CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyCallFramePrefix target=z80 hooks=.*call_frame_prefix.*call_frame_model=return_address_and_saved_fp" compiler.log
	grep -q "target_call_frame_prefix function=allocatorRaPolicyFrameVoidCallee role=layout target=z80 frame_end=-4 return_sp=-1 saved_fp_low=-3 saved_fp_high=-2" compiler.log
	grep -q "target_call_frame_prefix function=allocatorRaPolicyFrameByteCallee role=callee_return target=z80 frame_end=-4 return_sp=-1 saved_fp_low=-3 saved_fp_high=-2" compiler.log
	grep -q "target_call_frame_prefix function=allocatorRaPolicyFrameWordCallee role=callee_return target=z80 frame_end=-4 return_sp=-1 saved_fp_low=-3 saved_fp_high=-2" compiler.log
	grep -q "target_call_frame_prefix function=allocatorRaPolicyCallFramePrefix callee=allocatorRaPolicyFrameVoidCallee role=caller_setup target=z80 frame_end=-4 return_sp=-1 saved_fp_low=-3 saved_fp_high=-2" compiler.log
	grep -q "target_call_frame_prefix function=allocatorRaPolicyCallFramePrefix callee=allocatorRaPolicyFrameVoidCallee role=caller_restore target=z80 frame_end=-4 return_sp=-1 saved_fp_low=-3 saved_fp_high=-2" compiler.log
	grep -q "target_call_frame_prefix function=allocatorRaPolicyCallFramePrefix callee=allocatorRaPolicyFrameByteCallee role=caller_setup target=z80 frame_end=-4 return_sp=-1 saved_fp_low=-3 saved_fp_high=-2" compiler.log
	grep -q "target_call_frame_prefix function=allocatorRaPolicyCallFramePrefix callee=allocatorRaPolicyFrameByteCallee role=caller_restore target=z80 frame_end=-4 return_sp=-1 saved_fp_low=-3 saved_fp_high=-2" compiler.log
	grep -q "target_call_frame_prefix function=allocatorRaPolicyCallFramePrefix callee=allocatorRaPolicyFrameWordCallee role=caller_setup target=z80 frame_end=-4 return_sp=-1 saved_fp_low=-3 saved_fp_high=-2" compiler.log
	grep -q "target_call_frame_prefix function=allocatorRaPolicyCallFramePrefix callee=allocatorRaPolicyFrameWordCallee role=caller_restore target=z80 frame_end=-4 return_sp=-1 saved_fp_low=-3 saved_fp_high=-2" compiler.log
	: && ! grep -q "invalid call-frame prefix" compiler.log
	test "$$(grep -c "target_call_frame_prefix function=allocatorRaPolicyCallFramePrefix callee=.* role=caller_setup" compiler.log)" -eq 3
	test "$$(grep -c "target_call_frame_prefix function=allocatorRaPolicyCallFramePrefix callee=.* role=caller_restore" compiler.log)" -eq 3
	test "$$(grep -c "old stack frame address -> DE" main.asm)" -eq 4
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
