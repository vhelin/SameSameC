CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_active_slot_storage_test
	$(BT) -s main.ssc
	./core_active_slot_storage_test > core_active_slot_storage_test.log 2>&1
	for function in allocatorRaCoreActiveSlotStorageByte allocatorRaCoreActiveSlotStorageWord main; do grep -q "active_slot_storage function=$$function slots=5 bytes=100 status=complete" compiler.log || exit 1; grep -q "target_active_slots function=$$function target=z80 slots=5 status=complete bytes=100 storage=policy_sized" compiler.log || exit 1; grep -q "target_active_slots_release function=$$function target=z80 slots=5 bytes=100 status=complete" compiler.log || exit 1; done
	test $$(grep -c "active_slot_storage function=allocatorRaCoreActiveSlotStorage.*status=complete" compiler.log) -eq 2
	: && ! grep -q "target_active_slots.*status=out_of_memory" compiler.log
	awk '/active_slot_storage function=allocatorRaCoreActiveSlotStorageByte/{getline; if ($$0 ~ /target_active_slots function=allocatorRaCoreActiveSlotStorageByte/) found=1} END {exit !found}' compiler.log
	grep -q "active_slot_storage function=coreActiveSlotStorageOne slots=1 bytes=20 status=complete" core_active_slot_storage_test.log
	grep -q "active_slot_storage function=coreActiveSlotStorageThree slots=3 bytes=60 status=complete" core_active_slot_storage_test.log
	grep -q "active_slot_storage function=coreActiveSlotStorageFive slots=5 bytes=100 status=complete" core_active_slot_storage_test.log
	grep -q "active_slot_storage function=coreActiveSlotStorageSeven slots=7 bytes=140 status=complete" core_active_slot_storage_test.log
	grep -q "active_slot_storage function=coreActiveSlotStorageMaximum slots=2147483647 .*status=" core_active_slot_storage_test.log
	grep -q "active_slot_storage function=<null> slots=5 bytes=0 status=invalid_input" core_active_slot_storage_test.log
	grep -q "active_slot_storage function=coreActiveSlotStorageZero slots=0 bytes=0 status=invalid_input" core_active_slot_storage_test.log
	grep -q "active_slot_storage function=coreActiveSlotStorageNegative slots=-1 bytes=0 status=invalid_input" core_active_slot_storage_test.log
	grep -q "active_slot_storage function=coreActiveSlotStorageNullOutput slots=5 bytes=0 status=invalid_input" core_active_slot_storage_test.log
	grep -q "active_slots_reset function=coreActiveSlotStorageLifecycle block=4 slots=7 status=complete" core_active_slot_storage_test.log
	test $$(grep -c "active_slot_assign function=coreActiveSlotStorageLifecycle" core_active_slot_storage_test.log) -eq 7
	grep -q "active_slots_expire function=coreActiveSlotStorageLifecycle block=4 instruction=46 slots=7 expired=7 status=complete" core_active_slot_storage_test.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_active_slot_storage_test: core_active_slot_storage_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_active_slot_storage_test core_active_slot_storage_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_active_slot_storage_test core_active_slot_storage_test.log