CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "join_path_reservation_commit function=allocatorRaCompareCRelational .*reservations=4 .*committed=4 .*commit=committed status=complete" compiler.log)" -eq 4
	test "$$(grep -c "join_assignment_apply_entry function=allocatorRaCompareCRelational temp=r2 .*phy=C status=complete" compiler.log)" -eq 12
	for instruction in 23 47 71 95; do grep -q "join_assignment_apply_entry function=allocatorRaCompareCRelational temp=r2 assignment=2 role=consumer instruction=$$instruction operand=1 phy=C status=complete" compiler.log || exit 1; done
	test "$$(grep -c "block_exit_spill_decision function=allocatorRaCompareCRelational .*temp=r2 .*spill=exempt" compiler.log)" -eq 15
	test "$$(grep -c "compare_8bit function=allocatorRaCompareCRelational.*op_name=JUMP_EQ.*arg1_kind=PHY_C.*arg2_kind=CONST" compiler.log)" -eq 4
	test $$(grep -c "retained arg1 in C" main.asm) -eq 8
	! grep -q "retained arg1 in B" main.asm
	test $$(grep -c "SUB A,B" main.asm) -ge 4
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log