CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
OFILES = main.asm


all: linked.sms core_candidate_dispatch_test
	$(BT) -s main.ssc
	./core_candidate_dispatch_test > core_candidate_dispatch_test.log 2>&1
	grep -q "candidate_dispatch function=allocatorRaCoreCandidateDispatchPath.*plan=3.*reason=clobber_between action=2.*path=yes overlap=yes status=complete" compiler.log
	grep -q "candidate_dispatch function=allocatorRaCoreCandidateDispatchConflict.*plan=4.*reason=active_register_conflict action=2.*path=yes overlap=yes status=complete" compiler.log
	grep -q "candidate_dispatch function=allocatorRaCoreCandidateDispatchUnsupported.*plan=2.*reason=unsupported_primary action=2.*path=no overlap=no status=complete" compiler.log
	grep -q "candidate_dispatch function=coreCandidateDispatch.*plan=1.*reason=primary_available action=1 phy=11 selected=-1 path=no overlap=no status=complete" core_candidate_dispatch_test.log
	grep -q "candidate_dispatch function=coreCandidateDispatch.*plan=2.*reason=unsupported_primary action=2 phy=22 selected=1 path=no overlap=no status=complete" core_candidate_dispatch_test.log
	grep -q "candidate_dispatch function=coreCandidateDispatch.*plan=3.*reason=clobber_between action=2 phy=22 selected=1 path=yes overlap=yes status=complete" core_candidate_dispatch_test.log
	grep -q "candidate_dispatch function=coreCandidateDispatch.*plan=4.*reason=active_register_conflict action=1 phy=11 selected=-1 path=yes overlap=yes status=complete" core_candidate_dispatch_test.log
	grep -q "candidate_dispatch function=coreCandidateDispatchDependency.*status=dependency_failed" core_candidate_dispatch_test.log
	test $$(grep -c "candidate_dispatch function=.*status=invalid_callback" core_candidate_dispatch_test.log) -eq 3
	test $$(grep -c "candidate_dispatch function=.*status=invalid_input" core_candidate_dispatch_test.log) -eq 8
	: && ! grep -q "candidate_dispatch function=allocatorRaCoreCandidateDispatch.*status=invalid" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_candidate_dispatch_test: core_candidate_dispatch_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_candidate_dispatch_test core_candidate_dispatch_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_candidate_dispatch_test core_candidate_dispatch_test.log
