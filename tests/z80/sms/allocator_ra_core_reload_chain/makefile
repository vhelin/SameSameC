CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_reload_chain_test
	$(BT) -s main.ssc
	./core_reload_chain_test > core_reload_chain_test.log 2>&1
	grep -q "reload_graph function=allocatorRaCoreReloadChainByte.*reloads=1.*truncated=no qualification=ready.*status=complete" compiler.log
	grep -q "reload_graph function=allocatorRaCoreReloadChainWord.*reloads=1.*truncated=no qualification=ready.*status=complete" compiler.log
	grep -q "reload_graph_preflight function=allocatorRaCoreReloadChainByte.*reloads=1 truncated=no mode=cfg_graph status=complete" compiler.log
	grep -q "reload_graph_preflight function=allocatorRaCoreReloadChainWord.*reloads=1 truncated=no mode=cfg_graph status=complete" compiler.log
	grep -q "reload_graph_transaction function=allocatorRaCoreReloadChainByte.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log
	grep -q "reload_graph_transaction function=allocatorRaCoreReloadChainWord.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadChainByte.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadChainWord.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "reload_chain function=coreChainThree.*count=3 capacity=4 truncated=no.*reason=block_end status=complete" core_reload_chain_test.log
	grep -q "reload_chain function=coreChainCapacity.*count=2 capacity=2 truncated=yes stop=6 reason=capacity status=complete" core_reload_chain_test.log
	grep -q "reload_chain function=coreChainInactive.*count=1.*status=complete" core_reload_chain_test.log
	grep -q "reload_chain function=coreChainRedefined.*count=1.*stop=3 reason=redefined status=complete" core_reload_chain_test.log
	grep -q "reload_chain function=coreChainIneligible.*count=1.*stop=3 reason=read_ineligible status=complete" core_reload_chain_test.log
	grep -q "reload_chain function=coreChainReadWrite.*count=1.*stop=2 reason=redefined_after_read status=complete" core_reload_chain_test.log
	grep -q "reload_chain function=coreChainEmpty.*count=0.*reason=block_end status=complete" core_reload_chain_test.log
	grep -q "reload_scan_range function=coreRangeFallthrough start_block=0 end_block=2 start=0 end=6 extended=2 reason=function_end status=complete" core_reload_chain_test.log
	grep -q "reload_scan_range function=coreRangeBranch start_block=0 end_block=1 start=0 end=4 extended=1 reason=branch status=complete" core_reload_chain_test.log
	grep -q "reload_scan_range function=coreRangeConditional start_block=0 end_block=1 start=0 end=4 extended=1 reason=branch status=complete" core_reload_chain_test.log
	grep -q "reload_scan_range function=coreRangeJump start_block=0 end_block=1 start=0 end=4 extended=1 reason=branch status=complete" core_reload_chain_test.log
	grep -q "reload_scan_range function=coreRangeJoin start_block=0 end_block=0 start=0 end=1 extended=0 reason=join status=complete" core_reload_chain_test.log
	grep -q "reload_scan_range function=coreRangeCycle start_block=0 end_block=0 start=0 end=1 extended=0 reason=cycle status=complete" core_reload_chain_test.log
	grep -q "reload_scan_range function=coreRangeNoncontiguous start_block=0 end_block=0 start=0 end=1 extended=0 reason=noncontiguous status=complete" core_reload_chain_test.log
	grep -q "reload_scan_range function=coreRangeBadEdge.*status=invalid_edge" core_reload_chain_test.log
	grep -q "reload_scan_range function=coreRangeBadBlock.*block=1 start=1 end=4 status=invalid_block" core_reload_chain_test.log
	grep -q "reload_chain function=<null>.*status=invalid_input" core_reload_chain_test.log
	grep -q "reload_chain function=coreChainZeroCapacity.*status=invalid_input" core_reload_chain_test.log
	grep -q "reload_chain function=coreChainNullOutput.*status=invalid_input" core_reload_chain_test.log
	grep -q "reload_chain function=coreChainNullResult.*status=invalid_input" core_reload_chain_test.log
	! grep -q "reload_chain function=allocatorRaCoreReloadChain.*status=invalid_input" compiler.log
	! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_reload_chain_test: core_reload_chain_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_reload_chain_test core_reload_chain_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_reload_chain_test core_reload_chain_test.log
