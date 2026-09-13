CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "struct_access_update function=allocatorRaStructPointerIncrementA op=increment.*final_type=4 address_uses=2" compiler.log
	grep -q "struct_access_update function=allocatorRaStructPointerDecrementHl op=decrement.*final_type=5 address_uses=2" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaStructPointerIncrementA.*phy=HL.*reason=multi_read_interval" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaStructPointerDecrementHl.*phy=HL.*reason=multi_read_interval" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaStructPointerIncrementA.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaStructPointerDecrementHl.*destination=existing_spill_slot" compiler.log
	grep -q "spill_insert_emit function=allocatorRaStructPointerIncrementA op=ARRAY_READ operand=arg1 phy=HL" compiler.log
	grep -q "spill_insert_emit function=allocatorRaStructPointerDecrementHl op=ARRAY_READ operand=arg1 phy=HL" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaStructPointerIncrementA.*operand=result.*phy=BC" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaStructPointerDecrementHl.*operand=result.*phy=BC" compiler.log
	grep -q "reload_insert_emit function=allocatorRaStructPointerIncrementA op=ARRAY_WRITE operand=result.*phy=BC" compiler.log
	grep -q "reload_insert_emit function=allocatorRaStructPointerDecrementHl op=ARRAY_WRITE operand=result.*phy=BC" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerIncrementA.*ARRAY_READ.*ADD arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerIncrementA.*ADD.*ARRAY_WRITE arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerDecrementHl.*ARRAY_READ.*SUB arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerDecrementHl.*SUB.*ARRAY_WRITE arg1" compiler.log
	grep -q "array_read_base function=allocatorRaStructPointerIncrementA kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "array_write_base function=allocatorRaStructPointerIncrementA kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "array_read_base function=allocatorRaStructPointerDecrementHl kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "array_write_base function=allocatorRaStructPointerDecrementHl kind=PHY_BC memory_pointer_load=no" compiler.log
	grep -q "preserve retained arg1 in spill slot" main.asm
	grep -q "reload result from spill slot into BC" main.asm
	grep -q "retained arg1 in HL" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log