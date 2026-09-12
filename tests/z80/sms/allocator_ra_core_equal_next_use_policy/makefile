CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_equal_next_use_policy_test
	$(BT) -s main.ssc
	./core_equal_next_use_policy_test > core_equal_next_use_policy_test.log 2>&1
	grep -q "equal_next_use_preference function=allocatorRaCoreEqualNextUsePolicyPreferred.*queried=yes prefer=yes reason=policy status=complete" compiler.log
	grep -q "linear_scan_choose function=allocatorRaCoreEqualNextUsePolicyPreferred.*prefer_equal=yes decision=replace_active reason=equal_next_use_preference status=complete" compiler.log
	grep -q "equal_next_use_preference function=allocatorRaCoreEqualNextUsePolicyUnequal.*queried=no prefer=no reason=unequal_next_use status=complete" compiler.log
	grep -q "equal_next_use_preference function=coreEqualPolicyPreferred.*queried=yes prefer=yes reason=policy status=complete" core_equal_next_use_policy_test.log
	grep -q "equal_next_use_preference function=coreEqualPolicyRejected.*queried=yes prefer=no reason=policy status=complete" core_equal_next_use_policy_test.log
	grep -q "equal_next_use_preference function=coreEqualPolicyUnequal.*queried=no prefer=no reason=unequal_next_use status=complete" core_equal_next_use_policy_test.log
	grep -q "equal_next_use_preference function=coreEqualPolicyBadCallback.*queried=yes prefer=invalid status=invalid_callback" core_equal_next_use_policy_test.log
	test $$(grep -c "equal_next_use_preference function=.*status=invalid_input" core_equal_next_use_policy_test.log) -eq 13
	! grep -q "equal_next_use_preference function=allocatorRaCoreEqualNextUsePolicy.*status=invalid" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_equal_next_use_policy_test: core_equal_next_use_policy_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_equal_next_use_policy_test core_equal_next_use_policy_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_equal_next_use_policy_test core_equal_next_use_policy_test.log
