CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_candidate_probe_test
	$(BT) -s main.ssc
	./core_candidate_probe_test > core_candidate_probe_test.log 2>&1
	grep -q "candidate_probe function=allocatorRaCoreProbeNearerA.*mode=before_decision.*eligible=yes rejection=none.*status=complete" compiler.log
	grep -q "candidate_probe function=allocatorRaCoreProbeNearerHl.*mode=before_decision.*eligible=yes rejection=none.*status=complete" compiler.log
	grep -q "candidate_range function=allocatorRaCoreProbeBoundary.*mode=before_decision eligible=no reason=consumer_at_decision status=complete" compiler.log
	grep -q "candidate_probe function=allocatorRaCoreProbeBoundary.*eligible=no rejection=range.*status=complete" compiler.log
	grep -q "candidate_range function=allocatorRaCoreProbePreservingOverlap.*mode=within_block eligible=yes reason=within_block status=complete" compiler.log
	grep -q "candidate_probe function=allocatorRaCoreProbePreservingOverlap.*mode=within_block.*eligible=yes rejection=none.*status=complete" compiler.log
	grep -q "candidate_range function=coreRangeBefore.*eligible=yes reason=consumer_before_decision status=complete" core_candidate_probe_test.log
	grep -q "candidate_range function=coreRangeAt.*eligible=no reason=consumer_at_decision status=complete" core_candidate_probe_test.log
	grep -q "candidate_range function=coreRangeAfter.*eligible=no reason=consumer_after_decision status=complete" core_candidate_probe_test.log
	grep -q "candidate_range function=coreRangeWithin.*eligible=yes reason=within_block status=complete" core_candidate_probe_test.log
	grep -q "candidate_qualification function=coreQualification.*eligible=yes rejection=none.*status=complete" core_candidate_probe_test.log
	grep -q "candidate_qualification function=coreQualification.*rejection=metadata deciding=0 status=complete" core_candidate_probe_test.log
	grep -q "candidate_qualification function=coreQualification.*rejection=interval deciding=2 status=complete" core_candidate_probe_test.log
	grep -q "candidate_qualification function=coreQualification.*rejection=legality deciding=2 status=complete" core_candidate_probe_test.log
	grep -q "candidate_qualification function=coreQualification.*rejection=transparency deciding=2 status=complete" core_candidate_probe_test.log
	grep -q "candidate_probe function=coreProbeEligible.*eligible=yes rejection=none deciding=-1 status=complete" core_candidate_probe_test.log
	grep -q "candidate_probe function=coreProbeRange.*eligible=no rejection=range deciding=2 status=complete" core_candidate_probe_test.log
	grep -q "candidate_probe function=coreProbeNotFound.*found=no eligible=no rejection=not_found deciding=-1 status=complete" core_candidate_probe_test.log
	grep -q "candidate_probe function=coreProbeBadCallback.*status=dependency_failed" core_candidate_probe_test.log
	test $$(grep -c "candidate_qualification function=badCallback.*status=invalid_callback" core_candidate_probe_test.log) -eq 4
	test $$(grep -c "candidate_range function=.*status=invalid_input" core_candidate_probe_test.log) -eq 5
	test $$(grep -c "candidate_qualification function=.*status=invalid_input" core_candidate_probe_test.log) -eq 5
	test $$(grep -c "candidate_probe function=.*status=invalid_input" core_candidate_probe_test.log) -eq 5
	: && ! grep -q "candidate_.*function=allocatorRaCoreProbe.*status=invalid" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_candidate_probe_test: core_candidate_probe_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_candidate_probe_test core_candidate_probe_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_candidate_probe_test core_candidate_probe_test.log
