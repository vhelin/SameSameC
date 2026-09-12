CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_primary_candidate_evaluation_test
	$(BT) -s main.ssc
	./core_primary_candidate_evaluation_test > core_primary_candidate_evaluation_test.log 2>&1
	grep -q "primary_candidate_evaluation function=allocatorRaCorePrimaryCandidateEvaluationPath.*phy=1 allowed=yes path_safe=no active=no status=complete" compiler.log
	grep -q "primary_candidate_evaluation function=allocatorRaCorePrimaryCandidateEvaluationConflict.*phy=2 allowed=yes path_safe=yes active=yes status=complete" compiler.log
	grep -q "primary_candidate_evaluation function=allocatorRaCorePrimaryCandidateEvaluationUnsupported.*phy=2 allowed=no path_safe=no active=no status=complete" compiler.log
	grep -q "primary_candidate_evaluation function=corePrimaryCandidateAvailable.*allowed=yes path_safe=yes active=no status=complete" core_primary_candidate_evaluation_test.log
	grep -q "primary_candidate_evaluation function=corePrimaryCandidateUnsupported.*allowed=no path_safe=no active=yes status=complete" core_primary_candidate_evaluation_test.log
	grep -q "primary_candidate_evaluation function=corePrimaryCandidateUnsafe.*allowed=yes path_safe=no active=yes status=complete" core_primary_candidate_evaluation_test.log
	grep -q "primary_candidate_evaluation function=corePrimaryCandidateBadAllowed.*invalid_fact=allowed status=invalid_callback" core_primary_candidate_evaluation_test.log
	grep -q "primary_candidate_evaluation function=corePrimaryCandidateBadPath.*invalid_fact=path_safe status=invalid_callback" core_primary_candidate_evaluation_test.log
	grep -q "primary_candidate_evaluation function=corePrimaryCandidateBadActive.*invalid_fact=active status=invalid_callback" core_primary_candidate_evaluation_test.log
	test $$(grep -c "primary_candidate_evaluation function=.*status=invalid_input" core_primary_candidate_evaluation_test.log) -eq 8
	! grep -q "primary_candidate_evaluation function=allocatorRaCorePrimaryCandidateEvaluation.*status=invalid" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_primary_candidate_evaluation_test: core_primary_candidate_evaluation_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_primary_candidate_evaluation_test core_primary_candidate_evaluation_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_primary_candidate_evaluation_test core_primary_candidate_evaluation_test.log
