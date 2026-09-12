CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "target_address_materialization target=IY kind=GLOBAL" compiler.log)" -eq 5
	test "$$(grep -c "target_specialized_address_materialization.*emitter=function_call_argument_source" compiler.log)" -eq 5
	test "$$(grep -c "function=allocatorRaPolicyCallGlobalCache emitter=function_call_argument_source" compiler.log)" -eq 2
	test "$$(grep -c "function=allocatorRaPolicyCallGlobalTransport emitter=function_call_argument_source" compiler.log)" -eq 3
	grep -q "function=allocatorRaPolicyCallGlobalCache callee=allocatorRaPolicyCallGlobalCacheSink argument=2 source_type=4 target_type=4 source_kind=GLOBAL transport=byte target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallGlobalCache callee=allocatorRaPolicyCallGlobalCacheSink argument=4 source_type=5 target_type=5 source_kind=GLOBAL transport=word target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallGlobalTransport callee=allocatorRaPolicyCallGlobalTransportSink argument=1 source_type=2 target_type=3 source_kind=GLOBAL transport=sign_extend target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallGlobalTransport callee=allocatorRaPolicyCallGlobalTransportSink argument=2 source_type=4 target_type=5 source_kind=GLOBAL transport=zero_extend target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallGlobalTransport callee=allocatorRaPolicyCallGlobalTransportSink argument=3 source_type=5 target_type=4 source_kind=GLOBAL transport=truncate target=z80" compiler.log
	grep -q "function=allocatorRaPolicyCallGlobalCache.*argument=1 kind=STACK_LOCAL offset=-4.*cache_before=reset cache_action=initialize cache_after=ready" compiler.log
	grep -q "function=allocatorRaPolicyCallGlobalCache.*argument=3 kind=STACK_LOCAL offset=-5.*cache_before=reset cache_action=initialize cache_after=ready" compiler.log
	grep -q "function=allocatorRaPolicyCallGlobalCache.*argument=5 kind=STACK_LOCAL offset=-6.*cache_before=reset cache_action=initialize cache_after=ready" compiler.log
	! grep -q "target_specialized_address_materialization.*emitter=function_call_argument_source.*mode=invalid" compiler.log
	! grep -q "_load_location_address_to_iy" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log