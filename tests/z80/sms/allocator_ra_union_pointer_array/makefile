CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

OFILES = main.asm

all: linked.sms
	test "$$(grep -c "root=g_allocator_union_pointer_array access=array root_kind=union_pointer_array" compiler.log)" -eq 4
	test "$$(grep -c "struct_access_indirect function=allocatorRaUnionPointerArray.*depth=1.*access=pointer" compiler.log)" -eq 4
	grep -q "linear_scan retain.*function=allocatorRaUnionPointerArrayWrite.*ARRAY_READ.*ARRAY_WRITE result" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaUnionPointerArrayRead.*ARRAY_READ.*XOR arg1" compiler.log
	grep -q "struct_access_update function=allocatorRaUnionPointerArrayIncrement op=increment.*split_candidate=yes" compiler.log
	grep -q "struct_access_update function=allocatorRaUnionPointerArrayDecrement op=decrement.*split_candidate=yes" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaUnionPointerArrayIncrement.*phy=HL" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaUnionPointerArrayDecrement.*phy=HL" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaUnionPointerArrayIncrement.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaUnionPointerArrayDecrement.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaUnionPointerArrayIncrement.*phy=BC" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaUnionPointerArrayDecrement.*phy=BC" compiler.log
	grep -q "spill_insert_emit function=allocatorRaUnionPointerArrayIncrement.*spill_offset=-8 bytes=2" compiler.log
	grep -q "spill_insert_emit function=allocatorRaUnionPointerArrayDecrement.*spill_offset=-9 bytes=2" compiler.log
	grep -q "reload_insert_emit function=allocatorRaUnionPointerArrayIncrement.*source_offset=-8 bytes=2 phy=BC" compiler.log
	grep -q "reload_insert_emit function=allocatorRaUnionPointerArrayDecrement.*source_offset=-9 bytes=2 phy=BC" compiler.log
	grep -q "array_write_base function=allocatorRaUnionPointerArrayWrite kind=PHY_HL.*phy=HL" compiler.log
	grep -q "array_read_base function=allocatorRaUnionPointerArrayRead kind=PHY_HL.*phy=HL" compiler.log
	grep -q "array_write_base function=allocatorRaUnionPointerArrayIncrement kind=PHY_BC.*phy=BC" compiler.log
	grep -q "array_write_base function=allocatorRaUnionPointerArrayDecrement kind=PHY_BC.*phy=BC" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc

linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

clean:
	rm -f $(OFILES) linked.sms compiler.log
