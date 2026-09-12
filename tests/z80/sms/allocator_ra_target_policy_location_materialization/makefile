CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyLocationB target=z80 hooks=.*location_materialization,address_materialization.*location_model=semantic_source_to_target_kind.*address_model=target_register_location_and_cache_profile" compiler.log
	grep -q "target_location_materialization function=allocatorRaPolicyLocationConstant operand=arg2 source=constant kind=CONST offset=0 size=1 phy=NONE target=z80" compiler.log
	grep -q "target_location_materialization function=allocatorRaPolicyLocationB operand=result source=global kind=GLOBAL offset=999999 size=1 phy=NONE target=z80" compiler.log
	grep -q "target_location_materialization function=allocatorRaPolicyLocationB operand=arg1 source=stack_local kind=STACK_LOCAL offset=-5 size=1 phy=NONE target=z80" compiler.log
	grep -q "target_location_materialization function=allocatorRaPolicyLocationSpill operand=result source=stack_spill kind=STACK_SPILL offset=-8 size=1 phy=NONE target=z80" compiler.log
	grep -q "target_location_materialization function=allocatorRaPolicyLocationB operand=result source=physical kind=PHY_A offset=0 size=1 phy=A target=z80" compiler.log
	grep -q "target_location_materialization function=allocatorRaPolicyLocationBc operand=result source=physical kind=PHY_HL offset=0 size=2 phy=HL target=z80" compiler.log
	grep -q "target_location_materialization function=allocatorRaPolicyLocationBc operand=result source=physical kind=PHY_BC offset=0 size=2 phy=BC target=z80" compiler.log
	grep -q "target_location_materialization function=allocatorRaPolicyLocationB operand=result source=physical kind=PHY_B offset=0 size=1 phy=B target=z80" compiler.log
	grep -q "target_location_materialization function=allocatorRaPolicyLocationB operand=result source=physical kind=PHY_C offset=0 size=1 phy=C target=z80" compiler.log
	test "$$(grep -c "target_location_materialization function=allocatorRaPolicyLocation" compiler.log)" -eq 80
	! grep -q "Target policy cannot materialize" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
