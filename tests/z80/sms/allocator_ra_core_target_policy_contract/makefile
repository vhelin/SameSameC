CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_target_policy_contract_test
	$(BT) -s main.ssc
	./core_target_policy_contract_test > core_target_policy_contract_test.log 2>&1
	grep -q "target_policy_validate function=allocatorRaCoreTargetPolicyByte target=z80 hooks=23 status=complete" compiler.log
	grep -q "target_policy_validate function=allocatorRaCoreTargetPolicyWord target=z80 hooks=23 status=complete" compiler.log
	awk '/target_policy_validate function=allocatorRaCoreTargetPolicyByte/{getline; if ($$0 ~ /target_policy function=allocatorRaCoreTargetPolicyByte/) found=1} END {exit !found}' compiler.log
	awk '/target_policy_validate function=allocatorRaCoreTargetPolicyWord/{getline; if ($$0 ~ /target_policy function=allocatorRaCoreTargetPolicyWord/) found=1} END {exit !found}' compiler.log
	! grep -q "target_policy_validate function=allocatorRaCoreTargetPolicy.*status=invalid" compiler.log
	grep -q "target_policy_validate function=coreTargetPolicyComplete target=opaque hooks=23 status=complete" core_target_policy_contract_test.log
	grep -q "target_policy_validate function=<null> target=opaque hooks=0 status=invalid_input" core_target_policy_contract_test.log
	grep -q "target_policy_validate function=coreTargetPolicyNull target=<null> hooks=0 status=invalid_input" core_target_policy_contract_test.log
	grep -q "target_policy_validate function=coreTargetPolicyNullName target=<null> hooks=0 status=invalid_input" core_target_policy_contract_test.log
	grep -q "target_policy_validate function=coreTargetPolicyEmptyName target=<null> hooks=0 status=invalid_input" core_target_policy_contract_test.log
	test $$(grep -c "target_policy_validate function=coreTargetPolicyMissing target=opaque hooks=23 missing=.*status=invalid_policy" core_target_policy_contract_test.log) -eq 23
	for hook in physical_register_units active_slot_count active_slot_index active_slot_name candidate_register_count candidate_register candidate_fallback equal_next_use_preference candidate_legality tac_clobbers tac_transparency call_result_register call_boundary_spill stack_return_transport return_slot_access call_frame_prefix stack_argument_layout argument_transport argument_byte_access location_materialization address_materialization split_spill split_reload; do grep -q "missing=$$hook status=invalid_policy" core_target_policy_contract_test.log || exit 1; done


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_target_policy_contract_test: core_target_policy_contract_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_target_policy_contract_test core_target_policy_contract_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_target_policy_contract_test core_target_policy_contract_test.log