CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

OFILES = main.asm

all: linked.sms
	grep -q "struct_access_address function=allocatorRaStructPointerArrayIncrement.*root_kind=struct_pointer_array" compiler.log
	grep -q "struct_access_address function=allocatorRaStructPointerArrayDecrement.*root_kind=struct_pointer_array" compiler.log
	grep -q "struct_access_update function=allocatorRaStructPointerArrayIncrement op=increment.*address_uses=2 read_index=0 write_index=0 split_candidate=yes" compiler.log
	grep -q "struct_access_update function=allocatorRaStructPointerArrayDecrement op=decrement.*address_uses=2 read_index=0 write_index=0 split_candidate=yes" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaStructPointerArrayIncrement.*phy=HL.*reason=multi_read_interval" compiler.log
	grep -q "linear_scan split_spill function=allocatorRaStructPointerArrayDecrement.*phy=HL.*reason=multi_read_interval" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaStructPointerArrayIncrement.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan spill_insert function=allocatorRaStructPointerArrayDecrement.*destination=existing_spill_slot" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaStructPointerArrayIncrement.*operand=result.*phy=BC" compiler.log
	grep -q "linear_scan reload_insert function=allocatorRaStructPointerArrayDecrement.*operand=result.*phy=BC" compiler.log
	grep -q "spill_insert_emit function=allocatorRaStructPointerArrayIncrement.*spill_offset=-8 bytes=2" compiler.log
	grep -q "spill_insert_emit function=allocatorRaStructPointerArrayDecrement.*spill_offset=-9 bytes=2" compiler.log
	grep -q "reload_insert_emit function=allocatorRaStructPointerArrayIncrement.*source_offset=-8 bytes=2 phy=BC" compiler.log
	grep -q "reload_insert_emit function=allocatorRaStructPointerArrayDecrement.*source_offset=-9 bytes=2 phy=BC" compiler.log
	grep -q "array_write_base function=allocatorRaStructPointerArrayIncrement kind=PHY_BC.*phy=BC" compiler.log
	grep -q "array_write_base function=allocatorRaStructPointerArrayDecrement kind=PHY_BC.*phy=BC" compiler.log
	grep -q "preserve retained arg1 in spill slot" main.asm
	grep -q "reload result from spill slot into BC" main.asm
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc

linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

clean:
	rm -f $(OFILES) linked.sms compiler.log
