CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_candidate_registers_test
	$(BT) -s main.ssc
	./core_candidate_registers_test > core_candidate_registers_test.log 2>&1
	for function in allocatorRaCoreCandidateRegistersByte allocatorRaCoreCandidateRegistersWord main; do grep -q "candidate_registers function=$$function target=z80 size=8 capacity=3 count=3 status=complete" compiler.log || exit 1; grep -q "candidate_registers function=$$function target=z80 size=16 capacity=2 count=2 status=complete" compiler.log || exit 1; done
	for candidate in "size=8 candidate=0 phy=1" "size=8 candidate=1 phy=5" "size=8 candidate=2 phy=4" "size=16 candidate=0 phy=2" "size=16 candidate=1 phy=3"; do grep -q "candidate_register function=allocatorRaCoreCandidateRegistersByte target=z80 $$candidate status=complete" compiler.log || exit 1; done
	awk '/candidate_registers function=allocatorRaCoreCandidateRegistersByte.*size=16.*status=complete/{getline; if ($$0 ~ /active_slot_role function=allocatorRaCoreCandidateRegistersByte/) found=1} END {exit !found}' compiler.log
	: && ! grep -q "candidate_registers function=allocatorRaCoreCandidateRegisters.*status=invalid" compiler.log
	grep -q "candidate_registers function=coreCandidateRegistersByte target=opaque-candidates size=8 capacity=3 count=3 status=complete" core_candidate_registers_test.log
	grep -q "candidate_register function=coreCandidateRegistersByte.*candidate=1 phy=-7 status=complete" core_candidate_registers_test.log
	grep -q "candidate_registers function=coreCandidateRegistersWord target=opaque-candidates size=16 capacity=3 count=2 status=complete" core_candidate_registers_test.log
	grep -q "candidate_register function=coreCandidateRegistersDuplicate.*duplicate_candidate=0 status=invalid_policy" core_candidate_registers_test.log
	grep -q "candidate_register function=coreCandidateRegistersSentinel.*phy=-999 status=invalid_policy" core_candidate_registers_test.log
	grep -q "candidate_registers function=coreCandidateRegistersZeroCount.*count=0 status=invalid_policy" core_candidate_registers_test.log
	grep -q "candidate_registers function=coreCandidateRegistersNegativeCount.*count=-1 status=invalid_policy" core_candidate_registers_test.log
	grep -q "candidate_registers function=coreCandidateRegistersOverCapacity.*capacity=3 count=4 status=invalid_policy" core_candidate_registers_test.log
	test $$(grep -c "candidate_registers function=.*status=invalid_input" core_candidate_registers_test.log) -eq 10


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_candidate_registers_test: core_candidate_registers_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_candidate_registers_test core_candidate_registers_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_candidate_registers_test core_candidate_registers_test.log