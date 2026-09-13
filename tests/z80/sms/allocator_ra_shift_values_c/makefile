CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

OFILES = main.asm

all: linked.sms
	grep -q "linear_scan alternate.*function=allocatorRaShiftValueC.*primary=A alternate=C.*producer_tac=.*consumer_tac=" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaShiftValueC.*8-bit in C.*producer_tac=.*ADD.*consumer_tac=.*SHIFT_LEFT arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaShiftValueC.*8-bit in C.*producer_tac=.*XOR.*consumer_tac=.*SHIFT_RIGHT arg1" compiler.log
	test "$$(grep -c "resolve function=allocatorRaShiftValueC operand=arg1 kind=PHY_C.*phy=C" compiler.log)" -eq 2
	test "$$(grep -c "resolve function=allocatorRaShiftValueC operand=arg2 kind=PHY_A.*phy=A" compiler.log)" -eq 2
	test "$$(grep -c "retained arg1 in C" main.asm)" -ge 2
	test "$$(grep -c "LD  L,C" main.asm)" -eq 2
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc

linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

clean:
	rm -f $(OFILES) linked.sms compiler.log
