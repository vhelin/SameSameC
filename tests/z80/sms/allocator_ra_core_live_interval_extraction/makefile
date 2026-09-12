CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_live_interval_test
	$(BT) -s main.ssc
	./core_live_interval_test > core_live_interval_test.log 2>&1
	test "$$(grep -c "interval_reset function=allocatorRaCoreIntervalArrayWrite capacity=8192 status=complete" compiler.log)" -eq 1
	grep -q "interval_build function=allocatorRaCoreIntervalArrayWrite capacity=8192 intervals=4 algorithm=aggregate_access_range status=complete" compiler.log
	grep -q "interval_note function=allocatorRaCoreIntervalArrayWrite temp=r1 size=16 instruction=13 access=write range=13..13 reads=0 writes=1 status=complete" compiler.log
	grep -q "interval_note function=allocatorRaCoreIntervalArrayWrite temp=r1 size=16 instruction=15 access=read range=13..15 reads=1 writes=1 status=complete" compiler.log
	grep -q "interval_note function=allocatorRaCoreIntervalArrayWrite temp=r3 size=16 instruction=12 access=read range=9..12 reads=2 writes=2 status=complete" compiler.log
	grep -q "interval_note function=allocatorRaCoreIntervalArrayWrite temp=r3 size=16 instruction=12 access=write range=9..12 reads=2 writes=3 status=complete" compiler.log
	grep -q "linear_scan retain function=allocatorRaCoreIntervalArrayWrite.*r1 16-bit in HL.*consumer_tac=15(ARRAY_WRITE result)" compiler.log
	grep -q "resolve function=allocatorRaCoreIntervalArrayWrite operand=result kind=PHY_HL" compiler.log
	test "$$(grep -c "interval_reset function=allocatorRaCoreIntervalReused capacity=8192 status=complete" compiler.log)" -eq 1
	grep -q "interval_note function=allocatorRaCoreIntervalReused temp=r0 size=8 instruction=30 access=read range=25..30 reads=2 writes=2 status=complete" compiler.log
	grep -q "interval_build function=allocatorRaCoreIntervalReused capacity=8192 intervals=1 algorithm=aggregate_access_range status=complete" compiler.log
	grep -q "temp_compaction stage=after_reuse function=allocatorRaCoreIntervalReused temps=1 highest_r=0" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaCoreIntervalReused.*producer_tac=25.*consumer_tac=26" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaCoreIntervalReused.*producer_tac=29.*consumer_tac=30" compiler.log
	! grep -q "interval_.*function=allocatorRaCoreInterval.*status=invalid_input" compiler.log
	grep -q "interval_note function=coreIntervals temp=r3 size=16 instruction=9 access=read range=2..9 reads=3 writes=1 status=complete" core_live_interval_test.log
	grep -q "interval_build function=coreIntervals capacity=5 intervals=2 algorithm=aggregate_access_range status=complete" core_live_interval_test.log
	grep -q "interval_note function=coreBadTemp.*status=invalid_input" core_live_interval_test.log
	grep -q "interval_note function=coreBadSize.*status=invalid_input" core_live_interval_test.log
	grep -q "interval_note function=coreBadInstruction.*status=invalid_input" core_live_interval_test.log
	grep -q "interval_note function=coreBadAccess.*access=invalid status=invalid_input" core_live_interval_test.log
	grep -q "interval_build function=coreNull capacity=5 status=invalid_input" core_live_interval_test.log
	grep -q "interval_reset function=coreNullReset capacity=5 status=invalid_input" core_live_interval_test.log
	grep -q "interval_build function=coreBadCapacity capacity=-1 status=invalid_input" core_live_interval_test.log
	grep -q "interval_build function=coreReset capacity=5 intervals=0 algorithm=aggregate_access_range status=complete" core_live_interval_test.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_live_interval_test: core_live_interval_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_live_interval_test core_live_interval_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_live_interval_test core_live_interval_test.log
