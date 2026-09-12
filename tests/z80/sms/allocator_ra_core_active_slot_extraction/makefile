CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_active_slot_test
	$(BT) -s main.ssc
	./core_active_slot_test > core_active_slot_test.log 2>&1
	grep -q "active_slot_storage function=allocatorRaCoreActiveSlotByte slots=5 bytes=100 status=complete" compiler.log
	grep -q "target_active_slots function=allocatorRaCoreActiveSlotByte target=z80 slots=5 status=complete bytes=100 storage=policy_sized" compiler.log
	grep -q "target_active_slots_release function=allocatorRaCoreActiveSlotByte target=z80 slots=5 bytes=100 status=complete" compiler.log
	grep -q "active_slots_reset function=allocatorRaCoreActiveSlotByte block=1 slots=5 status=complete" compiler.log
	grep -q "active_slot_assign function=allocatorRaCoreActiveSlotByte block=1 slot=0 temp=r0 producer=6 next=9.*status=complete" compiler.log
	grep -q "active_slot_assign function=allocatorRaCoreActiveSlotByte block=1 slot=4 temp=r1 producer=7 next=8.*status=complete" compiler.log
	grep -q "active_slots_expire function=allocatorRaCoreActiveSlotByte block=1 instruction=9 slots=5 expired=2 status=complete" compiler.log
	grep -q "active_slot_assign function=allocatorRaCoreActiveSlotWord block=1 slot=1 temp=r1 producer=22 next=23.*status=complete" compiler.log
	grep -q "active_slot_expire function=allocatorRaCoreActiveSlotWord block=1 instruction=23 slot=1 temp=r1 next=23 status=expired" compiler.log
	grep -q "active_slots_reset function=coreSlots block=2 slots=5 status=complete" core_active_slot_test.log
	test `grep -c "active_slot_assign function=coreSlots" core_active_slot_test.log` -eq 6
	grep -q "active_slots_expire function=coreSlots block=2 instruction=10 slots=5 expired=1 status=complete" core_active_slot_test.log
	grep -q "active_slots_expire function=coreSlots block=2 instruction=12 slots=5 expired=3 status=complete" core_active_slot_test.log
	test `grep -c "active_slot_assign function=coreAllExpire" core_active_slot_test.log` -eq 5
	grep -q "active_slot_assign function=coreAllExpire.*slot=0.*producer=30 next=31 operand=0.*status=complete" core_active_slot_test.log
	grep -q "active_slot_assign function=coreAllExpire.*slot=4.*operand=104.*status=complete" core_active_slot_test.log
	grep -q "active_slots_expire function=coreAllExpire block=4 instruction=31 slots=5 expired=5 status=complete" core_active_slot_test.log
	grep -q "active_slots_expire function=coreTransactional.*slot=4.*status=invalid_state" core_active_slot_test.log
	grep -q "active_slots_reset function=<null>.*status=invalid_input" core_active_slot_test.log
	grep -q "active_slot_assign function=coreBadAssign.*retained=invalid status=invalid_input" core_active_slot_test.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_active_slot_test: core_active_slot_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_active_slot_test core_active_slot_test.c ../../../../compiler/register_allocator.c


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_active_slot_test core_active_slot_test.log
