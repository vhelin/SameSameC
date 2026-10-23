CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain function=allocatorRaMulResultA.*producer_tac=.*MUL" compiler.log
	grep -q "linear_scan retain function=allocatorRaDivResultA.*producer_tac=.*DIV" compiler.log
	grep -q "linear_scan retain function=allocatorRaModResultA.*producer_tac=.*MOD" compiler.log
	grep -q "linear_scan retain function=allocatorRaMulResultHl.*producer_tac=.*MUL" compiler.log
	grep -q "linear_scan retain function=allocatorRaDivResultHl.*producer_tac=.*DIV" compiler.log
	grep -q "linear_scan retain function=allocatorRaModResultHl.*producer_tac=.*MOD" compiler.log
	grep -q "retained result in A" main.asm
	grep -q "retained arg1 in A" main.asm
	grep -q "retained result in HL" main.asm
	grep -q "retained arg1 in HL" main.asm
	: && ! grep -Eq "reason=unmigrated_emitter.*(MUL|DIV|MOD)" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log