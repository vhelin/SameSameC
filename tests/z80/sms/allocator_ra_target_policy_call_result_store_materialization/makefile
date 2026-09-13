CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "target_specialized_address_materialization.*emitter=function_call_result_store" compiler.log)" -eq 3
	grep -q "function=allocatorRaPolicyCallResultStoreGlobal emitter=function_call_result_store operand=result kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallResultStoreLocal emitter=function_call_result_store operand=result kind=STACK_LOCAL offset=-9 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallResultStoreFar emitter=function_call_result_store operand=result kind=STACK_LOCAL offset=-149 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function_call_return function=allocatorRaPolicyCallResultStoreGlobal.*kind=GLOBAL size=2 phy=NONE var_type=5 promoted_type=5" compiler.log
	grep -q "function_call_return function=allocatorRaPolicyCallResultStoreLocal.*kind=STACK_LOCAL size=2 phy=NONE var_type=5 promoted_type=5" compiler.log
	grep -q "function_call_return function=allocatorRaPolicyCallResultStoreRetained.*kind=PHY_HL size=2 phy=HL" compiler.log
	: && ! grep -q "function=allocatorRaPolicyCallResultStoreRetained emitter=function_call_result_store" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*emitter=function_call_result_store.*mode=invalid" compiler.log
	: && ! grep -q "_load_location_address_to_iy" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log