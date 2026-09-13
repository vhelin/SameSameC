CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan alternate function=allocatorRaCompareCNeqBOverlap.*primary=A alternate=C" compiler.log
	test "$$(grep -c "join_assignment_apply_entry function=allocatorRaCompareCNeqBOverlap temp=r2 .*phy=C status=complete" compiler.log)" -eq 3
	grep -q "join_assignment_apply_entry function=allocatorRaCompareCNeqBOverlap temp=r2 assignment=2 role=consumer instruction=23 operand=1 phy=C status=complete" compiler.log
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCompareCNeqBOverlap .*temp=r2 .*spill=exempt" compiler.log)" -eq 3
	grep -q "linear_scan retain.*function=allocatorRaCompareCNeqBOverlap.*8-bit in C.*XOR arg1" compiler.log
	grep -q "compare_8bit function=allocatorRaCompareCNeqBOverlap.*arg1_kind=PHY_C.*arg2_kind=CONST" compiler.log
	grep -q "retained arg1 in C" main.asm
	test "$$(grep -c "retained arg1 in C" main.asm)" -eq 2
	: && ! grep -q "retained arg1 in B" main.asm
	grep -q "SUB A,B" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log