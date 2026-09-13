CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_path_analysis_test
	$(BT) -s main.ssc
	./core_path_analysis_test > core_path_analysis_test.log 2>&1
	grep -q "interval_blocking function=allocatorRaCorePathFartherA.*blocked=no reason=no_spill_reason status=complete" compiler.log
	grep -q "transparent_range function=allocatorRaCorePathFartherA.*transparent=yes.*reason=all_transparent status=complete" compiler.log
	grep -q "candidate_path function=allocatorRaCorePathFartherA.*safe=yes.*reason=nearer_candidate status=complete" compiler.log
	grep -q "candidate_path function=allocatorRaCorePathBoundaryA.*safe=no.*reason=non_transparent status=complete" compiler.log
	grep -q "linear_scan bc_transparent_candidate function=allocatorRaCorePathPreservingOverlap" compiler.log
	grep -q "candidate_path function=allocatorRaCorePathPreservingOverlap.*safe=yes.*reason=all_transparent status=complete" compiler.log
	grep -q "interval_blocking function=coreIntervalBetween.*blocked=yes reason=boundary_between status=complete" core_path_analysis_test.log
	grep -q "interval_blocking function=coreIntervalUnconditional.*blocked=yes reason=unconditional_spill_reason status=complete" core_path_analysis_test.log
	grep -q "transparent_range function=coreRangeBlocked.*transparent=no blocking=2 reason=target_non_transparent status=complete" core_path_analysis_test.log
	grep -q "transparent_range function=coreRangeBadCallback.*status=invalid_callback" core_path_analysis_test.log
	grep -q "candidate_path function=corePathNearer.*safe=yes deciding=2 reason=nearer_candidate status=complete" core_path_analysis_test.log
	grep -q "candidate_path function=corePathBlocked.*safe=no deciding=2 reason=non_transparent status=complete" core_path_analysis_test.log
	grep -q "candidate_path function=corePathBadNearer.*invalid_predicate=nearer_candidate status=invalid_callback" core_path_analysis_test.log
	grep -q "candidate_path function=corePathBadOverlap.*invalid_predicate=preserving_overlap status=invalid_callback" core_path_analysis_test.log
	grep -q "candidate_path function=corePathBadSpecial.*invalid_predicate=special_transparency status=invalid_callback" core_path_analysis_test.log
	grep -q "candidate_path function=corePathBadTarget.*invalid_predicate=target_transparency status=invalid_callback" core_path_analysis_test.log
	test $$(grep -c "interval_blocking function=.*status=invalid_input" core_path_analysis_test.log) -eq 7
	test $$(grep -c "transparent_range function=.*status=invalid_input" core_path_analysis_test.log) -eq 9
	test $$(grep -c "candidate_path function=.*status=invalid_input" core_path_analysis_test.log) -eq 13
	: && ! grep -q "interval_blocking function=allocatorRaCorePath.*status=invalid" compiler.log
	: && ! grep -q "transparent_range function=allocatorRaCorePath.*status=invalid" compiler.log
	: && ! grep -q "candidate_path function=allocatorRaCorePath.*status=invalid" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_path_analysis_test: core_path_analysis_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_path_analysis_test core_path_analysis_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_path_analysis_test core_path_analysis_test.log
