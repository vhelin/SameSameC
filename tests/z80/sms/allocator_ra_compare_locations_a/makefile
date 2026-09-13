CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "join_path_reservation_commit function=allocatorRaCompareLocationsA.*reservations=4 .*committed=4 .*commit=committed status=complete" compiler.log)" -eq 3
	test "$$(grep -c "join_assignment_apply_entry function=allocatorRaCompareLocationsA.*phy=C status=complete" compiler.log)" -eq 9
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCompareLocationsA.*spill=exempt" compiler.log)" -eq 9
	: && ! grep -q "join_assignment_apply_entry function=allocatorRaCompareLocationsARetainedArg2" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log