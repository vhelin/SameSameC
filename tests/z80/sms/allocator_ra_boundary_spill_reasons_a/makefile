CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	$(BT) -s main.ssc
	grep -q "migrated_emitter function=allocatorRaBoundarySpillReasonUnmigratedA.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaBoundarySpillReasonUnmigratedA.*consumer_tac=.*ARRAY_WRITE.*arg2" compiler.log
	: && ! grep -q "reason=unmigrated_emitter.*ARRAY_WRITE" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg2 in A" main.asm
	grep -q "return_boundary_consumer function=allocatorRaBoundarySpillReasonReturnA.*action=defer_to_candidate_policy" compiler.log
	grep -q "linear_scan retain function=allocatorRaBoundarySpillReasonReturnA.*8-bit in A.*consumer_tac=.*RETURN_VALUE arg1" compiler.log
	grep -q "return_value_source function=allocatorRaBoundarySpillReasonReturnA kind=PHY_A size=1 phy=A" compiler.log
	grep -q "; spill: function_call" main.asm
	: && ! grep -q "; spill: return" main.asm
	: && ! grep -q "; spill: unmigrated_emitter" main.asm


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log