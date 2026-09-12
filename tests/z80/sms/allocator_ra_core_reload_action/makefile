CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_reload_action_test
	$(BT) -s main.ssc
	./core_reload_action_test > core_reload_action_test.log 2>&1
	grep -q "reload_mutation function=allocatorRaCoreReloadActionByte.*action=insert reload_phy=3 apply=yes status=complete" compiler.log
	grep -q "reload_mutation function=allocatorRaCoreReloadActionWord.*action=insert reload_phy=3 apply=yes status=complete" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadActionByte.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadActionWord.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "reload_action function=coreReloadActionInsert.*action=insert reason=target_register_selected status=complete" core_reload_action_test.log
	grep -q "reload_action function=coreReloadActionUnsupported.*action=none reason=target_unsupported status=complete" core_reload_action_test.log
	grep -q "reload_action function=<null>.*status=invalid_input" core_reload_action_test.log
	grep -q "reload_action function=coreReloadActionEqual.*status=invalid_input" core_reload_action_test.log
	grep -q "reload_action function=coreReloadActionOpaqueInsert.*reload_phy=0 action=insert reason=target_register_selected status=complete" core_reload_action_test.log
	grep -q "reload_action function=coreReloadActionOpaqueNone.*reload_phy=-1 action=none reason=target_unsupported status=complete" core_reload_action_test.log
	! grep -q "reload_action function=allocatorRaCoreReloadAction.*status=invalid_input" compiler.log
	! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_reload_action_test: core_reload_action_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_reload_action_test core_reload_action_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_reload_action_test core_reload_action_test.log