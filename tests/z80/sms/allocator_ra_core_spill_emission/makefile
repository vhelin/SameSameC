CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_spill_emission_test
	$(BT) -s main.ssc
	./core_spill_emission_test > core_spill_emission_test.log 2>&1
	grep -q "spill_emission function=allocatorRaCoreSpillEmissionByte requested=yes temp=r0 phy=2 spill=yes destination_offset=-8 bytes=2 emit=yes status=complete" compiler.log
	grep -q "spill_emission function=allocatorRaCoreSpillEmissionWord requested=yes temp=r0 phy=2 spill=yes destination_offset=-9 bytes=2 emit=yes status=complete" compiler.log
	test $$(grep -c "spill_emission function=allocatorRaCoreSpillEmission.*emit=yes status=complete" compiler.log) -eq 2
	grep -q "spill_insert_emit function=allocatorRaCoreSpillEmissionByte.*phy=HL spill_offset=-8 bytes=2" compiler.log
	grep -q "spill_insert_emit function=allocatorRaCoreSpillEmissionWord.*phy=HL spill_offset=-9 bytes=2" compiler.log
	grep -q "spill_emission function=coreSpillEmissionWord requested=yes temp=r7 phy=2 spill=yes destination_offset=-12 bytes=2 emit=yes status=complete" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionByte requested=yes temp=r8 phy=2 spill=yes destination_offset=5 bytes=1 emit=yes status=complete" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionSkip requested=no emit=no status=complete" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionOpaque requested=yes temp=r9 phy=0 spill=yes destination_offset=-32768 bytes=4 emit=yes status=complete" core_spill_emission_test.log
	grep -q "spill_emission function=<null>.*status=invalid_input" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionBadRequest.*requested=invalid.*status=invalid_input" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionBadTemp.*status=invalid_input" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionNoRegister.*status=invalid_input" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionBadSpill.*spill=invalid.*status=invalid_input" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionNoSpill.*spill=no.*status=invalid_input" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionZeroBytes.*bytes=0 status=invalid_input" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionNegativeBytes.*bytes=-1 status=invalid_input" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionStaleSkip requested=no.*status=invalid_input" core_spill_emission_test.log
	grep -q "spill_emission function=coreSpillEmissionNullOutput.*status=invalid_input" core_spill_emission_test.log
	: && ! grep -q "spill_emission function=allocatorRaCoreSpillEmission.*status=invalid_input" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_spill_emission_test: core_spill_emission_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_spill_emission_test core_spill_emission_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_spill_emission_test core_spill_emission_test.log