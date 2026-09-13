CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan overlap_check function=allocatorRaIndexCRetentionAConflict.*candidate=C.*candidate_units=C.*conflict=no" compiler.log
	grep -q "linear_scan alternate function=allocatorRaIndexCRetentionAConflict.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan retain function=allocatorRaIndexCRetentionAConflict.*8-bit in C.*ARRAY_WRITE arg2" compiler.log
	grep -q "linear_scan retain function=allocatorRaIndexCRetentionAConflict.*8-bit in A.*ARRAY_WRITE arg1" compiler.log
	grep -q "resolve function=allocatorRaIndexCRetentionAConflict operand=arg2 kind=PHY_C" compiler.log
	grep -q "array_write_index function=allocatorRaIndexCRetentionAConflict kind=PHY_C" compiler.log
	grep -q "array_write_value function=allocatorRaIndexCRetentionAConflict kind=PHY_A" compiler.log
	grep -q "register 0 size 1 spill no phy C" main.asm
	grep -q "register 1 size 1 spill no phy A" main.asm
	grep -q "retained result in C" main.asm
	grep -q "retained result in A" main.asm
	grep -q "retained arg2 in C" main.asm
	grep -q "retained arg1 in A" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log