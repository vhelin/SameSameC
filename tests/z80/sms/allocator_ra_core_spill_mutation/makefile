CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_spill_mutation_test
	$(BT) -s main.ssc
	./core_spill_mutation_test > core_spill_mutation_test.log 2>&1
	grep -q "spill_mutation function=allocatorRaCoreSpillMutationByte.*split_action=preserve retained=yes phy=2 apply=yes reason=preserved_interval status=complete" compiler.log
	grep -q "spill_mutation function=allocatorRaCoreSpillMutationWord.*split_action=preserve retained=yes phy=2 apply=yes reason=preserved_interval status=complete" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaCoreSpillMutationByte.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreSpillMutationWord.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "spill_emission function=allocatorRaCoreSpillMutationByte requested=yes.*destination_offset=-8 bytes=2 emit=yes status=complete" compiler.log
	grep -q "spill_emission function=allocatorRaCoreSpillMutationWord requested=yes.*destination_offset=-9 bytes=2 emit=yes status=complete" compiler.log
	grep -q "spill_mutation function=coreSpillMutationApply.*apply=yes reason=preserved_interval status=complete" core_spill_mutation_test.log
	grep -q "spill_mutation function=coreSpillMutationNone.*apply=no reason=split_not_required status=complete" core_spill_mutation_test.log
	grep -q "spill_mutation function=coreSpillMutationNotRetained.*apply=no reason=candidate_not_retained status=complete" core_spill_mutation_test.log
	grep -q "spill_mutation function=coreSpillMutationOpaque.*operand=0.*phy=0 apply=yes" core_spill_mutation_test.log
	grep -q "spill_mutation function=<null>.*status=invalid_input" core_spill_mutation_test.log
	grep -q "spill_mutation function=coreSpillMutationRejectedAction.*split_action=invalid.*status=invalid_input" core_spill_mutation_test.log
	grep -q "spill_mutation function=coreSpillMutationBadRetained.*retained=invalid.*status=invalid_input" core_spill_mutation_test.log
	grep -q "spill_mutation function=coreSpillMutationNoRegister.*status=invalid_input" core_spill_mutation_test.log
	grep -q "spill_mutation function=coreSpillMutationNullOutput.*status=invalid_input" core_spill_mutation_test.log
	! grep -q "spill_mutation function=allocatorRaCoreSpillMutation.*status=invalid_input" compiler.log
	! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_spill_mutation_test: core_spill_mutation_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_spill_mutation_test core_spill_mutation_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_spill_mutation_test core_spill_mutation_test.log