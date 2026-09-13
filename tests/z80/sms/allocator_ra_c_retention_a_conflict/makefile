CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan overlap_check function=allocatorRaCRetentionAConflict.*candidate=C.*candidate_units=C.*active_A=no.*active_C=no.*conflict=no" compiler.log
	grep -q "linear_scan alternate function=allocatorRaCRetentionAConflict.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan retain function=allocatorRaCRetentionAConflict.*8-bit in C.*consumer_tac=.*XOR arg1" compiler.log
	grep -q "linear_scan retain function=allocatorRaCRetentionAConflict.*8-bit in A.*consumer_tac=.*XOR arg2" compiler.log
	grep -q "resolve function=allocatorRaCRetentionAConflict operand=result kind=PHY_C" compiler.log
	grep -q "resolve function=allocatorRaCRetentionAConflict operand=arg1 kind=PHY_C" compiler.log
	grep -q "retained result in C" main.asm
	grep -q "retained arg1 in C" main.asm
	grep -q "retained arg2 in A" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log