CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan retain function=allocatorRaFunctionCallResultA.*producer_tac=.*FUNCTION_CALL_USE_RETURN_VALUE.*consumer_tac=.*XOR.*arg1" compiler.log
	grep -q "linear_scan retain function=allocatorRaFunctionCallResultHl.*producer_tac=.*FUNCTION_CALL_USE_RETURN_VALUE.*consumer_tac=.*XOR.*arg1" compiler.log
	grep -q "function_call_return function=allocatorRaFunctionCallResultA callee=allocatorRaCallResultCalleeA kind=PHY_A" compiler.log
	grep -q "function_call_return function=allocatorRaFunctionCallResultHl callee=allocatorRaCallResultCalleeHl kind=PHY_HL" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained call result in A" main.asm
	grep -q "retained call result in HL" main.asm
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