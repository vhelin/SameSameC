CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_split_action_test
	$(BT) -s main.ssc
	./core_split_action_test > core_split_action_test.log 2>&1
	grep -q "split_action function=allocatorRaCoreSplitByte.*single_read=no.*preservation_supported=yes.*action=preserve reason=multi_read_interval status=complete" compiler.log
	grep -q "split_action function=allocatorRaCoreSplitWord.*single_read=no.*preservation_supported=yes.*action=preserve reason=multi_read_interval status=complete" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaCoreSplitByte.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreSplitByte.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaCoreSplitWord.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreSplitWord.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "split_action function=coreSplitSingle.*action=none reason=single_read status=complete" core_split_action_test.log
	grep -q "split_action function=coreSplitPreserve.*action=preserve reason=multi_read_interval status=complete" core_split_action_test.log
	grep -q "split_action function=coreSplitUnsupported.*action=reject_unsupported reason=preservation_unsupported status=complete" core_split_action_test.log
	grep -q "split_action function=coreSplitRegisterChanged.*action=reject_register reason=preservation_register_changed status=complete" core_split_action_test.log
	grep -q "split_action function=<null>.*status=invalid_input" core_split_action_test.log
	grep -q "split_action function=coreSplitBadSingleRead.*status=invalid_input" core_split_action_test.log
	grep -q "split_action function=coreSplitNoSelected.*status=invalid_input" core_split_action_test.log
	grep -q "split_action function=coreSplitSingleWithPreservation.*status=invalid_input" core_split_action_test.log
	! grep -q "split_action function=allocatorRaCoreSplit.*status=invalid_input" compiler.log
	! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_split_action_test: core_split_action_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_split_action_test core_split_action_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_split_action_test core_split_action_test.log
