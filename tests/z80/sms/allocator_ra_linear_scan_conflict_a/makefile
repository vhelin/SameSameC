CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	$(BT) -s main.ssc
	grep -q "linear_scan alternate function=allocatorRaLinearScanConflictA.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan retain function=allocatorRaLinearScanConflictA.*8-bit in C.*consumer_tac=.*ADD arg1" compiler.log
	grep -q "linear_scan retain function=allocatorRaLinearScanConflictA.*8-bit in A.*consumer_tac=.*ADD arg2" compiler.log
	grep -q "resolve function=allocatorRaLinearScanConflictA operand=arg1 kind=PHY_C" compiler.log
	grep -q "retained arg1 in C" main.asm
	grep -q "retained arg2 in A" main.asm
	grep -q "register 0 size 1 spill no phy C" main.asm
	grep -q "register 1 size 1 spill no phy A" main.asm


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log