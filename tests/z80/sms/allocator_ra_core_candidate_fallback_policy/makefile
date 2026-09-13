CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_candidate_fallback_policy_test
	$(BT) -s main.ssc
	./core_candidate_fallback_policy_test > core_candidate_fallback_policy_test.log 2>&1
	grep -q "candidate_fallbacks function=allocatorRaCoreCandidateFallbackPolicyPath target=z80 size=8 candidates=3 unsupported=no path=yes conflict=yes status=complete" compiler.log
	grep -q "candidate_fallbacks function=allocatorRaCoreCandidateFallbackPolicyConflict target=z80 size=16 candidates=2 unsupported=yes path=no conflict=yes status=complete" compiler.log
	grep -q "candidate_fallbacks function=allocatorRaCoreCandidateFallbackPolicyUnsupported target=z80 size=16 candidates=2 unsupported=yes path=no conflict=yes status=complete" compiler.log
	grep -q "candidate_fallbacks function=coreFallbackPolicyByte target=opaque-fallback size=8 candidates=3 unsupported=no path=yes conflict=yes status=complete" core_candidate_fallback_policy_test.log
	grep -q "candidate_fallbacks function=coreFallbackPolicyWord target=opaque-fallback size=16 candidates=2 unsupported=yes path=no conflict=yes status=complete" core_candidate_fallback_policy_test.log
	grep -q "candidate_fallbacks function=coreFallbackPolicySingle target=opaque-fallback size=8 candidates=1 unsupported=no path=no conflict=no status=complete" core_candidate_fallback_policy_test.log
	test $$(grep -c "candidate_fallbacks function=.*status=invalid_callback" core_candidate_fallback_policy_test.log) -eq 3
	test $$(grep -c "candidate_fallbacks function=.*status=invalid_input" core_candidate_fallback_policy_test.log) -eq 10
	: && ! grep -q "candidate_fallbacks function=allocatorRaCoreCandidateFallbackPolicy.*status=invalid" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_candidate_fallback_policy_test: core_candidate_fallback_policy_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_candidate_fallback_policy_test core_candidate_fallback_policy_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_candidate_fallback_policy_test core_candidate_fallback_policy_test.log
