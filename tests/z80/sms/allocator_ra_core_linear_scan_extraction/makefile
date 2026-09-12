CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_linear_scan_test
	$(BT) -s main.ssc
	./core_linear_scan_test > core_linear_scan_test.log 2>&1
	grep -q "linear_scan_choose function=allocatorRaCoreLinearScanNearer.*slot=HL.*decision=replace_active reason=nearer_next_use status=complete" compiler.log
	grep -q "linear_scan spill function=allocatorRaCoreLinearScanNearer.*reason=farther_next_use" compiler.log
	grep -q "linear_scan_choose function=allocatorRaCoreLinearScanEqual.*slot=A.*prefer_equal=yes decision=replace_active reason=equal_next_use_preference status=complete" compiler.log
	grep -q "linear_scan prefer_array_write_value function=allocatorRaCoreLinearScanEqual.*reason=shared_next_use" compiler.log
	grep -q "linear_scan_choose function=coreEmpty.*decision=assign reason=empty_slot status=complete" core_linear_scan_test.log
	grep -q "linear_scan_choose function=coreExpired.*decision=assign reason=active_expired status=complete" core_linear_scan_test.log
	grep -q "linear_scan_choose function=coreNearer.*decision=replace_active reason=nearer_next_use status=complete" core_linear_scan_test.log
	grep -q "linear_scan_choose function=coreFarther.*decision=keep_active reason=active_nearer status=complete" core_linear_scan_test.log
	grep -q "linear_scan_choose function=coreEqualKeep.*decision=keep_active reason=equal_next_use status=complete" core_linear_scan_test.log
	grep -q "linear_scan_choose function=coreEqualReplace.*decision=replace_active reason=equal_next_use_preference status=complete" core_linear_scan_test.log
	grep -q "linear_scan_choose function=coreUnavailable.*active_replaceable=no.*decision=keep_active reason=active_unavailable status=complete" core_linear_scan_test.log
	grep -q "linear_scan_choose function=coreBadActive.*status=invalid_input" core_linear_scan_test.log
	grep -q "linear_scan_choose function=coreBadReplaceable.*active_replaceable=invalid.*status=invalid_input" core_linear_scan_test.log
	grep -q "linear_scan_choose function=<null>.*status=invalid_input" core_linear_scan_test.log
	grep -q "linear_scan_choose function=coreNullSlot.*slot=<null>.*status=invalid_input" core_linear_scan_test.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_linear_scan_test: core_linear_scan_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_linear_scan_test core_linear_scan_test.c ../../../../compiler/register_allocator.c


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_linear_scan_test core_linear_scan_test.log
