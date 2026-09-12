CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_physical_overlap_test
	$(BT) -s main.ssc
	./core_physical_overlap_test > core_physical_overlap_test.log 2>&1
	grep -q "physical_overlap function=allocatorRaCorePhysicalOverlapByte target=z80 left=4 left_units=2 right=5 right_units=4 overlap=no status=complete" compiler.log
	grep -q "physical_overlap function=allocatorRaCorePhysicalOverlapWord target=z80 left=3 left_units=6 right=2 right_units=24 overlap=no status=complete" compiler.log
	! grep -q "physical_overlap function=allocatorRaCorePhysicalOverlap.*status=invalid" compiler.log
	grep -q "overlap_check function=allocatorRaCorePhysicalOverlapByte.*candidate=B.*active_C=yes.*conflict=no" compiler.log
	grep -q "overlap_check function=allocatorRaCorePhysicalOverlapWord.*candidate=BC.*active_HL=yes.*conflict=no" compiler.log
	grep -q "physical_overlap function=corePhysicalOverlap target=opaque left=10 left_units=1 right=10 right_units=1 overlap=yes status=complete" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlap target=opaque left=10 left_units=1 right=12 right_units=3 overlap=yes status=complete" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlap target=opaque left=12 left_units=3 right=10 right_units=1 overlap=yes status=complete" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlap target=opaque left=10 left_units=1 right=11 right_units=2 overlap=no status=complete" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlap target=opaque left=13 left_units=1024 right=12 right_units=3 overlap=no status=complete" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlapZeroLeft.*overlap=yes status=invalid_policy" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlapZeroRight.*overlap=yes status=invalid_policy" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlapNegative.*overlap=yes status=invalid_policy" core_physical_overlap_test.log
	grep -q "physical_overlap function=<null>.*overlap=yes status=invalid_input" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlapNullPolicy target=<null>.*status=invalid_input" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlapNullName target=<null>.*status=invalid_input" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlapEmptyName target=<null>.*status=invalid_input" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlapNoUnits.*status=invalid_input" core_physical_overlap_test.log
	grep -q "physical_overlap function=corePhysicalOverlapNullOutput.*status=invalid_input" core_physical_overlap_test.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_physical_overlap_test: core_physical_overlap_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_physical_overlap_test core_physical_overlap_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_physical_overlap_test core_physical_overlap_test.log