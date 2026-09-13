CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_candidate_evaluation_test
	$(BT) -s main.ssc
	./core_candidate_evaluation_test > core_candidate_evaluation_test.log 2>&1
	grep -q "candidate_evaluations function=allocatorRaCoreCandidateEvaluationByte.*reason=clobber_between.*candidates=2 capacity=2 status=complete" compiler.log
	grep -q "candidate_evaluation function=allocatorRaCoreCandidateEvaluationByte.*candidate=0 phy=5 allowed=yes path_safe=yes conflict_free=yes status=complete" compiler.log
	grep -q "candidate_evaluations function=allocatorRaCoreCandidateEvaluationWord.*reason=active_register_conflict.*candidates=1 capacity=2 status=complete" compiler.log
	grep -q "candidate_evaluations function=allocatorRaCoreCandidateEvaluationUnsupported.*reason=unsupported_primary.*candidates=1 capacity=2 status=complete" compiler.log
	grep -q "candidate_selection function=allocatorRaCoreCandidateEvaluationByte.*reason=clobber_between.*selected=0 phy=5 status=complete" compiler.log
	grep -q "candidate_selection function=allocatorRaCoreCandidateEvaluationWord.*reason=active_register_conflict.*selected=0 phy=3 status=complete" compiler.log
	grep -q "candidate_selection function=allocatorRaCoreCandidateEvaluationUnsupported.*reason=unsupported_primary.*selected=0 phy=3 status=complete" compiler.log
	grep -q "candidate_evaluation function=coreCandidateEvaluationLazy.*candidate=0 phy=41 allowed=no path_safe=no conflict_free=no status=complete" core_candidate_evaluation_test.log
	grep -q "candidate_evaluation function=coreCandidateEvaluationLazy.*candidate=1 phy=-7 allowed=yes path_safe=no conflict_free=no status=complete" core_candidate_evaluation_test.log
	grep -q "candidate_evaluation function=coreCandidateEvaluationLazy.*candidate=2 phy=88 allowed=yes path_safe=yes conflict_free=yes status=complete" core_candidate_evaluation_test.log
	grep -q "candidate_evaluations function=coreCandidateEvaluationLazy.*candidates=3 capacity=4 status=complete" core_candidate_evaluation_test.log
	grep -q "candidate_evaluation function=coreCandidateEvaluationBadAllowed.*allowed=invalid.*status=invalid_callback" core_candidate_evaluation_test.log
	grep -q "candidate_evaluation function=coreCandidateEvaluationBadPath.*path_safe=invalid.*status=invalid_callback" core_candidate_evaluation_test.log
	grep -q "candidate_evaluation function=coreCandidateEvaluationBadConflict.*conflict_free=invalid.*status=invalid_callback" core_candidate_evaluation_test.log
	grep -q "candidate_evaluation function=coreCandidateEvaluationSentinel.*phy=-999 status=invalid_input" core_candidate_evaluation_test.log
	grep -q "candidate_evaluation function=coreCandidateEvaluationDuplicate.*duplicate_candidate=0 status=invalid_input" core_candidate_evaluation_test.log
	test $$(grep "candidate_evaluations function=.*status=invalid_callback" core_candidate_evaluation_test.log | grep -vc "function=coreCandidateSelection") -eq 3
	grep -q "candidate_evaluations function=coreCandidateEvaluationNullContext.*status=invalid_input" core_candidate_evaluation_test.log
	test $$(grep "candidate_evaluations function=.*status=invalid_input" core_candidate_evaluation_test.log | grep -vc "function=coreCandidateSelection") -eq 14
	grep -q "candidate_selection function=coreCandidateSelectionThird.*selected=2 phy=88 status=complete" core_candidate_evaluation_test.log
	grep -q "candidate_selection function=coreCandidateSelectionNone.*selected=-1 phy=-999 status=complete" core_candidate_evaluation_test.log
	grep -q "candidate_selection function=coreCandidateSelectionFirst.*selected=0 phy=88 status=complete" core_candidate_evaluation_test.log
	test $$(grep -c "candidate_selection function=.*status=dependency_failed" core_candidate_evaluation_test.log) -eq 4
	test $$(grep -c "candidate_selection function=.*status=invalid_input" core_candidate_evaluation_test.log) -eq 4
	: && ! grep -q "candidate_evaluations function=allocatorRaCoreCandidateEvaluation.*status=invalid" compiler.log
	: && ! grep -q "candidate_selection function=allocatorRaCoreCandidateEvaluation.*status=invalid" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_candidate_evaluation_test: core_candidate_evaluation_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_candidate_evaluation_test core_candidate_evaluation_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_candidate_evaluation_test core_candidate_evaluation_test.log
