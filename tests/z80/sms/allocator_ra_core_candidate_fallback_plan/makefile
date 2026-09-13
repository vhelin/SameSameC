CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_candidate_fallback_plan_test
	$(BT) -s main.ssc
	./core_candidate_fallback_plan_test > core_candidate_fallback_plan_test.log 2>&1
	grep -q "candidate_fallback_plan function=allocatorRaCoreCandidateFallbackPath.*allowed=yes path_safe=no active=no.*plan=3 reason=fallback_path status=complete" compiler.log
	grep -q "candidate_fallback_plan function=allocatorRaCoreCandidateFallbackConflict.*allowed=yes path_safe=yes active=yes.*plan=4 reason=fallback_conflict status=complete" compiler.log
	grep -q "candidate_fallback_plan function=allocatorRaCoreCandidateFallbackUnsupported.*allowed=no.*plan=2 reason=fallback_unsupported status=complete" compiler.log
	grep -q "candidate_fallback_plan function=coreFallbackPrimary.*plan=1 reason=primary_available status=complete" core_candidate_fallback_plan_test.log
	grep -q "candidate_fallback_plan function=coreRejectUnsupported.*plan=5 reason=reject_unsupported status=complete" core_candidate_fallback_plan_test.log
	grep -q "candidate_fallback_plan function=coreRejectPath.*plan=6 reason=reject_path status=complete" core_candidate_fallback_plan_test.log
	grep -q "candidate_fallback_plan function=coreDeferConflict.*plan=1 reason=primary_conflict_deferred status=complete" core_candidate_fallback_plan_test.log
	grep -q "candidate_fallback_plan function=coreUnsupportedPrecedence.*plan=2 reason=fallback_unsupported status=complete" core_candidate_fallback_plan_test.log
	grep -q "candidate_fallback_plan function=corePathPrecedence.*plan=3 reason=fallback_path status=complete" core_candidate_fallback_plan_test.log
	test $$(grep -c "candidate_fallback_plan function=.*status=invalid_input" core_candidate_fallback_plan_test.log) -eq 8
	: && ! grep -q "candidate_fallback_plan function=allocatorRaCoreCandidateFallback.*status=invalid" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_candidate_fallback_plan_test: core_candidate_fallback_plan_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_candidate_fallback_plan_test core_candidate_fallback_plan_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_candidate_fallback_plan_test core_candidate_fallback_plan_test.log
