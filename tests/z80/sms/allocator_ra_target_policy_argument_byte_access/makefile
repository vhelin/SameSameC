CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS_RA = -mz80 -rs 3 -ra -o
CFLAGS_DEFAULT = -mz80 -rs 3 -o
CFLAGS_NO_RA = -mz80 -rs 3 -no-ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm default.asm no_ra.asm


all: linked.sms linked_default.sms linked_no_ra.sms
	grep -q "target_policy function=allocatorRaPolicyArgumentByteAccess target=z80 hooks=.*argument_byte_access.*argument_byte_model=logical_offsets" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentByteAccess callee=allocatorRaPolicyByteAccessSink argument=1 transport=byte source_low=0 source_high=-1 target_low=0 target_high=-1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentByteAccess callee=allocatorRaPolicyByteAccessSink argument=2 transport=word source_low=0 source_high=1 target_low=0 target_high=1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentByteAccess callee=allocatorRaPolicyByteAccessSink argument=3 transport=sign_extend source_low=0 source_high=-1 target_low=0 target_high=1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentByteAccess callee=allocatorRaPolicyByteAccessSink argument=4 transport=zero_extend source_low=0 source_high=-1 target_low=0 target_high=1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentByteAccess callee=allocatorRaPolicyByteAccessSink argument=5 transport=truncate source_low=0 source_high=1 target_low=0 target_high=-1 target=z80" compiler.log
	grep -q "target_argument_byte_access function=allocatorRaPolicyArgumentByteAccess callee=allocatorRaPolicyByteAccessSink argument=6 transport=word source_low=0 source_high=1 target_low=0 target_high=1 target=z80" compiler.log
	test "$$(grep -c "target_argument_byte_access function=allocatorRaPolicyArgumentByteAccess" compiler.log)" -eq 6
	: && ! grep -q "invalid byte access" compiler.log
	: && ! grep -q "target_argument_byte_access" default.log
	: && ! grep -q "target_argument_byte_access" no_ra.log
	cmp -s linked_default.sms linked_no_ra.sms
	test "$$(grep -c "copy argument" main.asm)" -eq 6
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: main.asm makefile
	$(LD) $(LDFLAGS) linked.sms main.asm

linked_default.sms: default.asm makefile
	$(LD) $(LDFLAGS) linked_default.sms default.asm

linked_no_ra.sms: no_ra.asm makefile
	$(LD) $(LDFLAGS) linked_no_ra.sms no_ra.asm

main.asm: main.ssc
	$(CC) $(CFLAGS_RA) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

default.asm: main.ssc
	$(CC) $(CFLAGS_DEFAULT) default.asm main.ssc > default.log 2>&1 || (cat default.log && false)

no_ra.asm: main.ssc
	$(CC) $(CFLAGS_NO_RA) no_ra.asm main.ssc > no_ra.log 2>&1 || (cat no_ra.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms linked_default.sms linked_no_ra.sms compiler.log default.log no_ra.log
