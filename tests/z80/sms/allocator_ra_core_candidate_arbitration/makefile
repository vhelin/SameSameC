CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_candidate_arbitration_test
	$(BT) -s main.ssc
	./core_candidate_arbitration_test > core_candidate_arbitration_test.log 2>&1
	grep -q "candidate_arbitration function=allocatorRaCoreCandidateArbitrationByte.*reason=clobber_between.*candidates=2 selected=0 phy=5 status=complete" compiler.log
	grep -q "candidate_arbitrate function=allocatorRaCoreCandidateArbitrationByte.*reason=clobber_between.*candidate=0 phy=5.*eligible=yes status=complete" compiler.log
	grep -q "candidate_arbitration function=allocatorRaCoreCandidateArbitrationWord.*reason=active_register_conflict.*candidates=1 selected=0 phy=3 status=complete" compiler.log
	grep -q "candidate_arbitration function=allocatorRaCoreCandidateArbitrationUnsupported.*reason=unsupported_primary.*candidates=1 selected=0 phy=3 status=complete" compiler.log
	grep -q "linear_scan alternate function=allocatorRaCoreCandidateArbitrationByte.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan alternate function=allocatorRaCoreCandidateArbitrationWord.*primary=HL alternate=BC reason=active_register_conflict" compiler.log
	grep -q "linear_scan retain function=allocatorRaCoreCandidateArbitrationUnsupported.*16-bit in BC.*consumer_tac=.*SHIFT_LEFT.*arg2" compiler.log
	grep -q "candidate_arbitration function=coreCandidateArbitrationFirst.*selected=0 phy=41 status=complete" core_candidate_arbitration_test.log
	grep -q "candidate_arbitrate function=coreCandidateArbitrationOrdered.*candidate=0 phy=41 allowed=no.*eligible=no status=complete" core_candidate_arbitration_test.log
	grep -q "candidate_arbitrate function=coreCandidateArbitrationOrdered.*candidate=1 phy=-7.*path_safe=no.*eligible=no status=complete" core_candidate_arbitration_test.log
	grep -q "candidate_arbitrate function=coreCandidateArbitrationOrdered.*candidate=2 phy=88.*conflict_free=no.*eligible=no status=complete" core_candidate_arbitration_test.log
	grep -q "candidate_arbitration function=coreCandidateArbitrationOrdered.*selected=3 phy=500 status=complete" core_candidate_arbitration_test.log
	grep -q "candidate_arbitration function=coreCandidateArbitrationNone.*selected=-1 phy=-999 status=complete" core_candidate_arbitration_test.log
	grep -q "candidate_arbitration function=coreCandidateArbitrationNegative.*selected=0 phy=-7 status=complete" core_candidate_arbitration_test.log
	grep -q "candidate_arbitrate function=coreCandidateArbitrationLateDuplicate.*duplicate_candidate=0 status=invalid_input" core_candidate_arbitration_test.log
	grep -q "candidate_arbitration function=coreCandidateArbitrationLateDuplicate.*selected=-1 phy=-999 status=invalid_input" core_candidate_arbitration_test.log
	test $$(grep -c "candidate_arbitration function=.*status=invalid_input" core_candidate_arbitration_test.log) -eq 14
	! grep -q "candidate_arbitration function=allocatorRaCoreCandidateArbitration.*status=invalid" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_candidate_arbitration_test: core_candidate_arbitration_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_candidate_arbitration_test core_candidate_arbitration_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_candidate_arbitration_test core_candidate_arbitration_test.log
