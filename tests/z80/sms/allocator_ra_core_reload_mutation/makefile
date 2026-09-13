CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_reload_mutation_test
	$(BT) -s main.ssc
	./core_reload_mutation_test > core_reload_mutation_test.log 2>&1
	grep -q "reload_mutation function=allocatorRaCoreReloadMutationByte.*action=insert reload_phy=3 apply=yes status=complete" compiler.log
	grep -q "reload_mutation function=allocatorRaCoreReloadMutationWord.*action=insert reload_phy=3 apply=yes status=complete" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadMutationByte.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadMutationWord.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "reload_mutation function=coreReloadMutationInsert.*apply=yes status=complete" core_reload_mutation_test.log
	grep -q "reload_mutation function=coreReloadMutationNone.*apply=no status=complete" core_reload_mutation_test.log
	grep -q "reload_mutation function=coreReloadMutationOpaqueInsert.*reload_phy=0 apply=yes status=complete" core_reload_mutation_test.log
	grep -q "reload_mutation function=<null>.*status=invalid_input" core_reload_mutation_test.log
	grep -q "reload_mutation function=coreReloadMutationBadAction.*action=invalid.*status=invalid_input" core_reload_mutation_test.log
	grep -q "reload_mutation function=coreReloadMutationNoneWithRegister.*status=invalid_input" core_reload_mutation_test.log
	grep -q "reload_mutation function=coreReloadMutationInsertWithoutRegister.*status=invalid_input" core_reload_mutation_test.log
	grep -q "reload_mutation function=coreReloadMutationNullOutput.*status=invalid_input" core_reload_mutation_test.log
	: && ! grep -q "reload_mutation function=allocatorRaCoreReloadMutation.*status=invalid_input" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_reload_mutation_test: core_reload_mutation_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_reload_mutation_test core_reload_mutation_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_reload_mutation_test core_reload_mutation_test.log