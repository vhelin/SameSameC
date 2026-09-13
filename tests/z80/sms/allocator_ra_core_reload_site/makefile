CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms core_reload_site_test
	$(BT) -s main.ssc
	./core_reload_site_test > core_reload_site_test.log 2>&1
	grep -q "reload_graph_site function=allocatorRaCoreReloadByte.*status=found" compiler.log
	grep -q "reload_graph_site function=allocatorRaCoreReloadWord.*status=found" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadByte.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaCoreReloadWord.*source=existing_spill_slot phy=BC" compiler.log
	grep -q "reload_site function=coreReloadFound.*instruction=2.*status=found" core_reload_site_test.log
	grep -q "reload_site function=coreReloadInactive.*instruction=4.*status=found" core_reload_site_test.log
	grep -q "reload_site function=coreReloadRedefined.*status=not_found reason=no_next_read" core_reload_site_test.log
	grep -q "reload_site function=coreReloadFirstIneligible.*instruction=2.*status=not_found reason=first_read_ineligible" core_reload_site_test.log
	grep -q "reload_site function=coreReloadReadWrite.*instruction=2.*status=found" core_reload_site_test.log
	grep -q "reload_site function=coreReloadNoRead.*status=not_found reason=no_next_read" core_reload_site_test.log
	grep -q "reload_site function=coreReloadEmpty.*status=not_found reason=no_next_read" core_reload_site_test.log
	grep -q "reload_site function=<null>.*status=invalid_input" core_reload_site_test.log
	grep -q "reload_site function=coreReloadBadStart.*status=invalid_input" core_reload_site_test.log
	grep -q "reload_site function=coreReloadNullEligible.*status=invalid_input" core_reload_site_test.log
	grep -q "reload_site function=coreReloadNullOutput.*status=invalid_input" core_reload_site_test.log
	: && ! grep -q "reload_site function=allocatorRaCoreReload.*status=invalid_input" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

core_reload_site_test: core_reload_site_test.c ../../../../compiler/register_allocator.c ../../../../compiler/register_allocator.h
	gcc -ansi -pedantic -Wall -Werror -o core_reload_site_test core_reload_site_test.c ../../../../compiler/register_allocator.c


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log core_reload_site_test core_reload_site_test.log
