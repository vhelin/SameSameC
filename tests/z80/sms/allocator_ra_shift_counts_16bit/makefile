CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaShiftLeftCountBc.*16-bit in BC.*producer_tac=.*ADD.*consumer_tac=.*SHIFT_LEFT.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaShiftRightCountBc.*16-bit in BC.*producer_tac=.*ADD.*consumer_tac=.*SHIFT_RIGHT.*arg2" compiler.log
	grep -q "resolve function=allocatorRaShiftLeftCountBc operand=arg2 kind=PHY_BC" compiler.log
	grep -q "resolve function=allocatorRaShiftRightCountBc operand=arg2 kind=PHY_BC" compiler.log
	grep -q "shift_count function=allocatorRaShiftLeftCountBc kind=PHY_BC" compiler.log
	grep -q "shift_count function=allocatorRaShiftRightCountBc kind=PHY_BC" compiler.log
	: && ! grep -q "reason=unmigrated_emitter.*SHIFT" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg2 in BC" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
