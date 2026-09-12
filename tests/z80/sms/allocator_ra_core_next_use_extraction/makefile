CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_next_use_test
	$(BT) -s main.ssc
	./core_next_use_test > core_next_use_test.log 2>&1
	test "$$(grep -c "register_allocator_core: next_use function=allocatorRaCoreNextUseByte" compiler.log)" -eq 7
	test "$$(grep -c "register_allocator_core: next_use function=allocatorRaCoreNextUseWord" compiler.log)" -eq 7
	test "$$(grep -c "register_allocator_core: next_use function=allocatorRaCoreNextUseReused" compiler.log)" -eq 4
	test "$$(grep -c "next_use function=allocatorRaCoreNextUseByte temp=r0 range=7..12 next=9 stop=read status=found" compiler.log)" -eq 2
	grep -q "next_use function=allocatorRaCoreNextUseByte temp=r1 range=8..9 next=8 stop=read status=found" compiler.log
	test "$$(grep -c "next_use function=allocatorRaCoreNextUseWord temp=r0 range=22..27 next=24 stop=read status=found" compiler.log)" -eq 2
	grep -q "next_use function=allocatorRaCoreNextUseWord temp=r1 range=23..24 next=23 stop=read status=found" compiler.log
	test "$$(grep -c "next_use function=allocatorRaCoreNextUseReused temp=r0 range=37..44 next=37 stop=read status=found" compiler.log)" -eq 2
	test "$$(grep -c "next_use function=allocatorRaCoreNextUseReused temp=r0 range=41..44 next=41 stop=read status=found" compiler.log)" -eq 2
	grep -q "temp_compaction stage=after_reuse function=allocatorRaCoreNextUseReused temps=1 highest_r=0" compiler.log
	grep -q "linear_scan competing_candidate function=allocatorRaCoreNextUseByte r1 producer_tac=7 next_tac=8 before_tac=9" compiler.log
	grep -q "linear_scan competing_candidate function=allocatorRaCoreNextUseWord r1 producer_tac=22 next_tac=23 before_tac=24" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaCoreNextUseByte.*producer_tac=7.*consumer_tac=8" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaCoreNextUseWord.*producer_tac=22.*consumer_tac=23" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaCoreNextUseReused.*producer_tac=36.*consumer_tac=37" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaCoreNextUseReused.*producer_tac=40.*consumer_tac=41" compiler.log
	! grep -q "register_allocator_core: next_use function=allocatorRaCoreNextUse.*status=invalid_range" compiler.log
	! grep -q "register_allocator_core: next_use function=allocatorRaCoreNextUse.*stop=redefined" compiler.log
	grep -q "next_use function=coreFound temp=r2 range=0..5 next=3 stop=read status=found" core_next_use_test.log
	grep -q "next_use function=coreRedefined temp=r2 range=0..2 next=none stop=redefined at=2 status=complete" core_next_use_test.log
	grep -q "next_use function=coreBlockEnd temp=r2 range=0..0 next=none stop=block_end status=complete" core_next_use_test.log
	grep -q "next_use function=coreEmpty temp=r2 range=6..5 next=none stop=block_end status=complete" core_next_use_test.log
	grep -q "next_use function=coreInvalid temp=r2 range=7..7 status=invalid_range" core_next_use_test.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_next_use_test: core_next_use_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_next_use_test core_next_use_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_next_use_test core_next_use_test.log
