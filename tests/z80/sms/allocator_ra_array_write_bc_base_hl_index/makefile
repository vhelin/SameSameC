CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "array_write_pointer_base_temp function=allocatorRaArrayWriteBcBaseHlIndex" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWriteBcBaseHlIndex.*producer_tac=.*XOR.*consumer_tac=.*ARRAY_WRITE.*arg2" compiler.log
	grep -q "linear_scan alternate function=allocatorRaArrayWriteBcBaseHlIndex.*primary=HL alternate=BC" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWriteBcBaseHlIndex.*producer_tac=.*ASSIGNMENT.*consumer_tac=.*ARRAY_WRITE.*result" compiler.log
	grep -q "resolve function=allocatorRaArrayWriteBcBaseHlIndex operand=result kind=PHY_BC" compiler.log
	grep -q "resolve function=allocatorRaArrayWriteBcBaseHlIndex operand=arg2 kind=PHY_HL" compiler.log
	grep -q "array_write_base function=allocatorRaArrayWriteBcBaseHlIndex kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "array_write_index function=allocatorRaArrayWriteBcBaseHlIndex kind=PHY_HL" compiler.log
	grep -q "retained result in BC" main.asm
	grep -q "retained arg2 in HL" main.asm
	: && ! grep -q "reason=unmigrated_emitter.*ARRAY_WRITE" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
