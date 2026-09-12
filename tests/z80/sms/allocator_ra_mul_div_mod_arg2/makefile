CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaMulArg2A.*producer_tac=.*ADD.*consumer_tac=.*MUL.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaDivArg2A.*producer_tac=.*ADD.*consumer_tac=.*DIV.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaModArg2A.*producer_tac=.*ADD.*consumer_tac=.*MOD.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaMulArg2Hl.*producer_tac=.*XOR.*consumer_tac=.*MUL.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaDivArg2Hl.*producer_tac=.*XOR.*consumer_tac=.*DIV.*arg2" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaModArg2Hl.*producer_tac=.*XOR.*consumer_tac=.*MOD.*arg2" compiler.log
	grep -q "resolve function=allocatorRaMulArg2A operand=arg2 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaDivArg2A operand=arg2 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaModArg2A operand=arg2 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaMulArg2Hl operand=arg2 kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaDivArg2Hl operand=arg2 kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaModArg2Hl operand=arg2 kind=PHY_HL" compiler.log
	grep -q "mul_div_mod_arg2 function=allocatorRaMulArg2A op=MUL kind=PHY_A" compiler.log
	grep -q "mul_div_mod_arg2 function=allocatorRaDivArg2A op=DIV kind=PHY_A" compiler.log
	grep -q "mul_div_mod_arg2 function=allocatorRaModArg2A op=MOD kind=PHY_A" compiler.log
	grep -q "mul_div_mod_arg2 function=allocatorRaMulArg2Hl op=MUL kind=PHY_HL" compiler.log
	grep -q "mul_div_mod_arg2 function=allocatorRaDivArg2Hl op=DIV kind=PHY_HL" compiler.log
	grep -q "mul_div_mod_arg2 function=allocatorRaModArg2Hl op=MOD kind=PHY_HL" compiler.log
	! grep -Eq "reason=unmigrated_emitter.*(MUL|DIV|MOD)" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg2 in A" main.asm
	grep -q "retained arg2 in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
