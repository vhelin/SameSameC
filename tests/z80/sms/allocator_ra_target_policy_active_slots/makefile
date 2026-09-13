CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_active_slots function=allocatorRaTargetPolicyByte target=z80 slots=5 status=complete" compiler.log
	grep -q "target_active_slots function=allocatorRaTargetPolicyWord target=z80 slots=5 status=complete" compiler.log
	awk '/target_active_slot function=allocatorRaTargetPolicyByte.*phy=A slot=0 name=A status=complete/ { seen=1 } seen && /active_slot_assign function=allocatorRaTargetPolicyByte.*slot=0 .*status=complete/ { found=1; exit } END { exit !found }' compiler.log
	awk '/target_active_slot function=allocatorRaTargetPolicyWord.*phy=HL slot=1 name=HL status=complete/ { seen=1 } seen && /active_slot_assign function=allocatorRaTargetPolicyWord.*slot=1 .*status=complete/ { found=1; exit } END { exit !found }' compiler.log
	awk '/target_active_slot function=allocatorRaTargetPolicyWord.*phy=BC slot=2 name=BC status=complete/ { seen=1 } seen && /active_slot_assign function=allocatorRaTargetPolicyWord.*slot=2 .*status=complete/ { found=1; exit } END { exit !found }' compiler.log
	awk '/target_active_slot function=allocatorRaTargetPolicyByte.*phy=B slot=3 name=B status=complete/ { seen=1 } seen && /active_slot_assign function=allocatorRaTargetPolicyByte.*slot=3 .*status=complete/ { found=1; exit } END { exit !found }' compiler.log
	awk '/target_active_slot function=allocatorRaTargetPolicyByte.*phy=C slot=4 name=C status=complete/ { seen=1 } seen && /active_slot_assign function=allocatorRaTargetPolicyByte.*slot=4 .*status=complete/ { found=1; exit } END { exit !found }' compiler.log
	grep -q "overlap_check function=allocatorRaTargetPolicyByte.*candidate=B.*active_C=yes.*conflict=no" compiler.log
	grep -q "overlap_check function=allocatorRaTargetPolicyWord.*candidate=BC.*active_HL=yes.*conflict=no" compiler.log
	: && ! grep -q "target_active_slot.*status=invalid_policy" compiler.log
	: && ! grep -q "target_active_slots.*status=invalid_policy" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
