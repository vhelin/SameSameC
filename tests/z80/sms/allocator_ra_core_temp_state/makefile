CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_temp_state_test
	$(BT) -s main.ssc
	./core_temp_state_test > core_temp_state_test.log 2>&1
	grep -q "temp_state_retain function=allocatorRaCoreTempStateSpillBacked.*storage=spill_backed status=complete" compiler.log
	grep -q "temp_state_spill function=allocatorRaCoreTempStateDisplace.*reason=farther_next_use.*spill=yes.*status=complete" compiler.log
	grep -q "temp_state_retain function=allocatorRaCoreTempStateRegisterOnly.*storage=register_only status=complete" compiler.log
	grep -q "temp_state_retain function=coreRegisterOnly.*storage=register_only status=complete" core_temp_state_test.log
	grep -q "temp_state_retain function=coreSpillConstraint.*storage=spill_backed status=complete" core_temp_state_test.log
	grep -q "temp_state_retain function=coreMultipleReads.*storage=spill_backed status=complete" core_temp_state_test.log
	grep -q "temp_state_spill function=coreDisplace.*reason=farther_next_use.*spill=yes.*status=complete" core_temp_state_test.log
	grep -q "temp_state_spill function=coreDisplaceRepeat.*prior_spill=yes.*status=complete" core_temp_state_test.log
	grep -q "temp_state_retain function=<null>.*status=invalid_input" core_temp_state_test.log
	grep -q "temp_state_retain function=coreBadBooleanState.*status=invalid_input" core_temp_state_test.log
	grep -q "temp_state_spill function=coreBadSpilledState.*status=invalid_input" core_temp_state_test.log
	grep -q "temp_state_spill function=coreNullReason.*status=invalid_input" core_temp_state_test.log
	: && ! grep -q "temp_state_.*function=allocatorRaCoreTempState.*status=invalid_input" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_temp_state_test: core_temp_state_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_temp_state_test core_temp_state_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_temp_state_test core_temp_state_test.log
