CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan overlap_check function=allocatorRaBRetentionCConflict.*candidate=C.*active_C=yes.*conflict=yes" compiler.log
	grep -q "linear_scan overlap_check function=allocatorRaBRetentionCConflict.*candidate=B.*candidate_units=B.*active_C=yes.*conflict=no" compiler.log
	grep -q "linear_scan alternate function=allocatorRaBRetentionCConflict.*primary=A alternate=B reason=clobber_between" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaBRetentionCConflict.*8-bit in B.*consumer_tac=.*XOR arg1" compiler.log
	grep -q "resolve function=allocatorRaBRetentionCConflict operand=result kind=PHY_B" compiler.log
	grep -q "resolve function=allocatorRaBRetentionCConflict operand=arg1 kind=PHY_B" compiler.log
	grep -q "register 0 size 1 spill no phy C" main.asm
	grep -q "register 1 size 1 spill yes phy NONE" main.asm
	grep -q "register 2 size 1 spill no phy A" main.asm
	grep -q "retained result in B" main.asm
	grep -q "retained arg1 in B" main.asm
	grep -q "retained arg1 in C" main.asm
	grep -q "XOR A,B" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log