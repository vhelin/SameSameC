CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain.*function=allocatorRaMulDivModResultC.*8-bit in C.*producer_tac=.*MUL.*consumer_tac=.*XOR arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaMulDivModResultC.*8-bit in C.*producer_tac=.*DIV.*consumer_tac=.*OR arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaMulDivModResultC.*8-bit in C.*producer_tac=.*MOD.*consumer_tac=.*XOR arg1" compiler.log
	grep -q "mul_div_mod_result function=allocatorRaMulDivModResultC op=MUL.*kind=PHY_C.*phy=C.*var_type=4.*promoted_type=4" compiler.log
	grep -q "mul_div_mod_result function=allocatorRaMulDivModResultC op=DIV.*kind=PHY_C.*phy=C.*var_type=4.*promoted_type=4" compiler.log
	grep -q "mul_div_mod_result function=allocatorRaMulDivModResultC op=MOD.*kind=PHY_C.*phy=C.*var_type=4.*promoted_type=4" compiler.log
	grep -q "LD  C,L" main.asm
	grep -q "LD  C,A" main.asm
	grep -q "LD  C,B" main.asm
	grep -q "retained result in C" main.asm
	grep -q "retained arg1 in C" main.asm
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log