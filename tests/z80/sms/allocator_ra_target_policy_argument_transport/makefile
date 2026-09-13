CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyArgumentTransport target=z80 hooks=.*argument_transport.*argument_transport_model=width_and_extension" compiler.log
	grep -q "target_argument_transport function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=1 source_type=4 target_type=4 source_kind=STACK_SPILL transport=byte target=z80" compiler.log
	grep -q "target_argument_transport function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=2 source_type=5 target_type=5 source_kind=STACK_SPILL transport=word target=z80" compiler.log
	grep -q "target_argument_transport function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=3 source_type=2 target_type=3 source_kind=STACK_LOCAL transport=sign_extend target=z80" compiler.log
	grep -q "target_argument_transport function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=4 source_type=4 target_type=5 source_kind=STACK_LOCAL transport=zero_extend target=z80" compiler.log
	grep -q "target_argument_transport function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=5 source_type=5 target_type=4 source_kind=STACK_LOCAL transport=truncate target=z80" compiler.log
	grep -q "target_argument_transport function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=6 source_type=3 target_type=5 source_kind=CONST transport=word target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=1 transport=byte source_low=0 source_high=-1 target_low=0 target_high=-1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=2 transport=word source_low=0 source_high=1 target_low=0 target_high=1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=3 transport=sign_extend source_low=0 source_high=-1 target_low=0 target_high=1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=4 transport=zero_extend source_low=0 source_high=-1 target_low=0 target_high=1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=5 transport=truncate source_low=0 source_high=1 target_low=0 target_high=-1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentTransport callee=allocatorRaPolicyTransportSink argument=6 transport=word source_low=0 source_high=1 target_low=0 target_high=1 target=z80" compiler.log
	test "$$(grep -c "target_argument_byte_access function=allocatorRaPolicyArgumentTransport" compiler.log)" -eq 6
	: && ! grep -q "Target policy cannot transport argument" compiler.log
	: && ! grep -q "invalid byte access" compiler.log
	test "$$(grep -c "copy argument" main.asm)" -eq 6
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
