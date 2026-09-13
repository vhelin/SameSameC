CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_mutation_apply_test
	$(BT) -s main.ssc
	./core_mutation_apply_test > core_mutation_apply_test.log 2>&1
	grep -q "spill_mutation_apply function=allocatorRaCoreMutationApplyByte.*apply=yes.*status=complete" compiler.log
	grep -q "reload_transaction_apply function=allocatorRaCoreMutationApplyByte.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log
	grep -q "spill_mutation_apply function=allocatorRaCoreMutationApplyWord.*apply=yes.*status=complete" compiler.log
	grep -q "reload_transaction_apply function=allocatorRaCoreMutationApplyWord.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaCoreMutationApplyByte.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreMutationApplyWord.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "spill_mutation_apply function=coreApplySpill.*status=complete" core_mutation_apply_test.log
	grep -q "spill_mutation_apply function=coreApplySpillEmpty.*status=skipped reason=empty_plan" core_mutation_apply_test.log
	grep -q "spill_mutation_apply function=coreApplySpillCallbackFail.*status=callback_failed" core_mutation_apply_test.log
	grep -q "reload_mutation_apply function=coreApplyReload.*status=complete" core_mutation_apply_test.log
	grep -q "reload_mutation_apply function=coreApplyReloadEmpty.*status=skipped reason=empty_plan" core_mutation_apply_test.log
	grep -q "reload_mutation_apply function=coreApplyReloadCallbackFail.*status=callback_failed" core_mutation_apply_test.log
	grep -q "spill_mutation_apply function=coreApplySpillBadEmpty.*status=invalid_input" core_mutation_apply_test.log
	grep -q "spill_mutation_apply function=coreApplySpillNullCallback.*status=invalid_input" core_mutation_apply_test.log
	grep -q "reload_mutation_apply function=coreApplyReloadBadEmpty.*status=invalid_input" core_mutation_apply_test.log
	grep -q "reload_mutation_apply function=coreApplyReloadNullPlan.*status=invalid_input" core_mutation_apply_test.log
	: && ! grep -q "mutation_apply function=allocatorRaCoreMutationApply.*status=invalid_input" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_mutation_apply_test: core_mutation_apply_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_mutation_apply_test core_mutation_apply_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_mutation_apply_test core_mutation_apply_test.log