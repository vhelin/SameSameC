CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_reload_emission_test
	$(BT) -s main.ssc
	./core_reload_emission_test > core_reload_emission_test.log 2>&1
	grep -q "reload_emission function=allocatorRaCoreReloadEmissionByte requested=yes.*phy=3 spill=yes.*bytes=2 emit=yes status=complete" compiler.log
	grep -q "reload_emission function=allocatorRaCoreReloadEmissionWord requested=yes.*phy=3 spill=yes.*bytes=2 emit=yes status=complete" compiler.log
	test $$(grep -c "reload_emission function=allocatorRaCoreReloadEmission.*emit=yes status=complete" compiler.log) -eq 2
	grep -q "reload_insert_emit function=allocatorRaCoreReloadEmissionByte.*bytes=2 phy=BC" compiler.log
	grep -q "reload_insert_emit function=allocatorRaCoreReloadEmissionWord.*bytes=2 phy=BC" compiler.log
	grep -q "reload_emission function=coreReloadEmissionWord requested=yes temp=r7 operand=0 phy=3 spill=yes source_offset=-12 bytes=2 emit=yes status=complete" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionByte requested=yes temp=r8 operand=0 phy=3 spill=yes source_offset=5 bytes=1 emit=yes status=complete" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionSkip requested=no emit=no status=complete" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionOpaque requested=yes temp=r9 operand=0 phy=0 spill=yes source_offset=-32768 bytes=4 emit=yes status=complete" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionArg1 requested=yes temp=r7 operand=1.*emit=yes status=complete" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionArg2 requested=yes temp=r7 operand=2.*emit=yes status=complete" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionResult requested=yes temp=r7 operand=0.*emit=yes status=complete" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionBadOperand.*operand=-1.*status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionSkipOperand.*operand=1.*status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionOperandSkip requested=no emit=no status=complete" core_reload_emission_test.log
	grep -q "reload_emission function=<null>.*status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionBadRequest.*requested=invalid.*status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionBadTemp.*status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionNoRegister.*status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionBadSpill.*spill=invalid.*status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionNoSpill.*spill=no.*status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionZeroBytes.*bytes=0 status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionNegativeBytes.*bytes=-1 status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionStaleSkip requested=no.*status=invalid_input" core_reload_emission_test.log
	grep -q "reload_emission function=coreReloadEmissionNullOutput.*status=invalid_input" core_reload_emission_test.log
	! grep -q "reload_emission function=allocatorRaCoreReloadEmission.*status=invalid_input" compiler.log
	! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_reload_emission_test: core_reload_emission_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_reload_emission_test core_reload_emission_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_reload_emission_test core_reload_emission_test.log