CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_active_slot_roles_test
	$(BT) -s main.ssc
	./core_active_slot_roles_test > core_active_slot_roles_test.log 2>&1
	for function in allocatorRaCoreActiveSlotRolesByte allocatorRaCoreActiveSlotRolesWord main; do grep -q "active_slot_roles function=$$function target=z80 roles=5 slots=5 status=complete" compiler.log || exit 1; done
	for role in "0 phy=1 slot=0 name=A" "1 phy=2 slot=1 name=HL" "2 phy=3 slot=2 name=BC" "3 phy=4 slot=3 name=B" "4 phy=5 slot=4 name=C"; do grep -q "active_slot_role function=allocatorRaCoreActiveSlotRolesByte target=z80 role=$$role status=complete" compiler.log || exit 1; done
	awk '/active_slot_roles function=allocatorRaCoreActiveSlotRolesByte.*status=complete/{getline; if ($$0 ~ /active_slot_storage function=allocatorRaCoreActiveSlotRolesByte/) found=1} END {exit !found}' compiler.log
	! grep -q "active_slot_roles function=.*status=invalid" compiler.log
	grep -q "active_slot_role function=coreActiveSlotRolesReordered target=reordered-seven role=0 phy=101 slot=4 name=opaque-a status=complete" core_active_slot_roles_test.log
	grep -q "active_slot_role function=coreActiveSlotRolesReordered target=reordered-seven role=2 phy=103 slot=6 name=opaque-pair status=complete" core_active_slot_roles_test.log
	grep -q "active_slot_roles function=coreActiveSlotRolesReordered target=reordered-seven roles=5 slots=7 status=complete" core_active_slot_roles_test.log
	grep -q "active_slot_role function=coreActiveSlotRolesDuplicate.*duplicate_role=0 status=invalid_policy" core_active_slot_roles_test.log
	grep -q "active_slot_role function=coreActiveSlotRolesOutOfRange.*slot=7.*status=invalid_policy" core_active_slot_roles_test.log
	grep -q "active_slot_role function=coreActiveSlotRolesNullName.*name=<null> status=invalid_policy" core_active_slot_roles_test.log
	grep -q "active_slot_role function=coreActiveSlotRolesEmptyName.*name= status=invalid_policy" core_active_slot_roles_test.log
	grep -q "active_slot_roles function=coreActiveSlotRolesZeroSlots.*slots=0 status=invalid_policy" core_active_slot_roles_test.log
	test $$(grep -c "active_slot_roles function=.*status=invalid_input" core_active_slot_roles_test.log) -eq 10
	grep -q "active_slot_role function=coreActiveSlotRolesUnknownRegister.*phy=999 slot=-1 name=<null> status=invalid_policy" core_active_slot_roles_test.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_active_slot_roles_test: core_active_slot_roles_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_active_slot_roles_test core_active_slot_roles_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_active_slot_roles_test core_active_slot_roles_test.log