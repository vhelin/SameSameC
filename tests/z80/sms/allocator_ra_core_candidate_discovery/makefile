CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_candidate_test
	$(BT) -s main.ssc
	./core_candidate_test > core_candidate_test.log 2>&1
	grep -q "candidate_discover function=allocatorRaCoreCandidateByte producer=9 temp=r0 consumer=11 operand=1 single_read=no block_end=.*read_scope=definition algorithm=definition_next_read status=found" compiler.log
	grep -q "candidate_discover function=allocatorRaCoreCandidateWord producer=26 temp=r0 consumer=28 operand=1 single_read=no block_end=.*read_scope=definition algorithm=definition_next_read status=found" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaCoreCandidateByte.*producer_tac=9 consumer_tac=11" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaCoreCandidateWord.*producer_tac=26 consumer_tac=28" compiler.log
	grep -q "candidate_discover function=allocatorRaCoreCandidateNonzero.*single_read=yes.*read_scope=definition algorithm=definition_next_read status=found" compiler.log
	grep -q "candidate_discover function=coreCandidateSingle producer=0 temp=r7 consumer=2 operand=77 single_read=yes block_end=3 read_scope=definition algorithm=definition_next_read status=found" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateMulti producer=0 temp=r7 consumer=2 operand=77 single_read=no block_end=3 read_scope=definition algorithm=definition_next_read status=found" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateReadWrite producer=0 temp=r7 consumer=2 operand=77 single_read=yes block_end=3 read_scope=definition algorithm=definition_next_read status=found" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateNoDefinition.*status=not_found reason=not_temp_definition" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateRedefined.*status=not_found reason=no_next_read" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateBlockEnd.*status=not_found reason=no_next_read" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateInactive producer=0 temp=r4 consumer=5 operand=9 single_read=yes" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateUnsupported.*status=not_found reason=unsupported_consumer" core_candidate_test.log
	grep -q "candidate_discover function=<null>.*status=invalid_input" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateBadRange.*status=invalid_input" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateNullCallback.*status=invalid_input" core_candidate_test.log
	grep -q "candidate_discover function=coreCandidateNullOutput.*status=invalid_input" core_candidate_test.log
	! grep -q "candidate_discover function=allocatorRaCoreCandidate.*status=invalid_input" compiler.log
	! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_candidate_test: core_candidate_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_candidate_test core_candidate_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_candidate_test core_candidate_test.log
