CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan alternate function=allocatorRaFunctionCallResultC.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaFunctionCallResultC.*8-bit in C.*producer_tac=.*FUNCTION_CALL_USE_RETURN_VALUE.*consumer_tac=.*XOR arg1" compiler.log
	grep -q "function_call_return function=allocatorRaFunctionCallResultC.*kind=PHY_C.*phy=C.*var_type=4.*promoted_type=4" compiler.log
	grep -q "linear_scan alternate function=allocatorRaFunctionCallOrResultC.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaFunctionCallOrResultC.*8-bit in C.*producer_tac=.*FUNCTION_CALL_USE_RETURN_VALUE.*consumer_tac=.*OR arg1" compiler.log
	grep -q "function_call_return function=allocatorRaFunctionCallOrResultC.*kind=PHY_C.*phy=C.*var_type=4.*promoted_type=4" compiler.log
	grep -q "retained call result in C" main.asm
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