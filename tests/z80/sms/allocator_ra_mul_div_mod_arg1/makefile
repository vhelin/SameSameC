CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaMulArg1A.*producer_tac=.*ADD.*consumer_tac=.*MUL.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaDivArg1A.*producer_tac=.*ADD.*consumer_tac=.*DIV.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaModArg1A.*producer_tac=.*ADD.*consumer_tac=.*MOD.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaMulArg1Hl.*producer_tac=.*XOR.*consumer_tac=.*MUL.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaDivArg1Hl.*producer_tac=.*XOR.*consumer_tac=.*DIV.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaModArg1Hl.*producer_tac=.*XOR.*consumer_tac=.*MOD.*arg1" compiler.log
	grep -q "resolve function=allocatorRaMulArg1A operand=arg1 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaDivArg1A operand=arg1 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaModArg1A operand=arg1 kind=PHY_A" compiler.log
	grep -q "resolve function=allocatorRaMulArg1Hl operand=arg1 kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaDivArg1Hl operand=arg1 kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaModArg1Hl operand=arg1 kind=PHY_HL" compiler.log
	grep -q "mul_div_mod_arg1 function=allocatorRaMulArg1A op=MUL kind=PHY_A" compiler.log
	grep -q "mul_div_mod_arg1 function=allocatorRaDivArg1A op=DIV kind=PHY_A" compiler.log
	grep -q "mul_div_mod_arg1 function=allocatorRaModArg1A op=MOD kind=PHY_A" compiler.log
	grep -q "mul_div_mod_arg1 function=allocatorRaMulArg1Hl op=MUL kind=PHY_HL" compiler.log
	grep -q "mul_div_mod_arg1 function=allocatorRaDivArg1Hl op=DIV kind=PHY_HL" compiler.log
	grep -q "mul_div_mod_arg1 function=allocatorRaModArg1Hl op=MOD kind=PHY_HL" compiler.log
	: && ! grep -Eq "reason=unmigrated_emitter.*(MUL|DIV|MOD)" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg1 in A" main.asm
	grep -q "retained arg1 in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
