CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "array_read_pointer_base_temp function=allocatorRaArrayReadBcBaseHlIndex" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayReadBcBaseHlIndex.*producer_tac=.*XOR.*consumer_tac=.*ARRAY_READ.*arg2" compiler.log
	grep -q "linear_scan alternate function=allocatorRaArrayReadBcBaseHlIndex.*primary=HL alternate=BC" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayReadBcBaseHlIndex.*ASSIGNMENT.*ARRAY_READ.*arg1" compiler.log
	grep -q "resolve function=allocatorRaArrayReadBcBaseHlIndex operand=arg1 kind=PHY_BC" compiler.log
	grep -q "array_read_base function=allocatorRaArrayReadBcBaseHlIndex kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "array_read_index function=allocatorRaArrayReadBcBaseHlIndex kind=PHY_HL" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg1 in BC" main.asm
	grep -q "retained arg2 in HL" main.asm
	grep -q "retained result in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log