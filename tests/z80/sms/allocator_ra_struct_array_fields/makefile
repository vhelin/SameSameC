CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "struct_access_address function=allocatorRaStructArrayFieldWriteA root=g_allocator_struct_array_field_values access=array" compiler.log
	grep -q "struct_access_address function=allocatorRaStructArrayFieldReadHl root=g_allocator_struct_array_field_values access=array" compiler.log
	grep -q "migrated_emitter function=allocatorRaStructArrayFieldWriteA.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaStructArrayFieldWriteA.*GET_ADDRESS.*ADD arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaStructArrayFieldWriteA.*ADD.*ARRAY_WRITE result" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructArrayFieldReadHl.*ADD.*ARRAY_READ arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructArrayFieldReadHl.*ARRAY_READ.*XOR arg1" compiler.log
	grep -q "resolve function=allocatorRaStructArrayFieldWriteA operand=result kind=PHY_HL" compiler.log
	grep -q "resolve function=allocatorRaStructArrayFieldReadHl operand=arg1 kind=PHY_HL" compiler.log
	grep -q "array_write_base function=allocatorRaStructArrayFieldWriteA kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "array_read_base function=allocatorRaStructArrayFieldReadHl kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "register 1 size 2 spill no phy HL" main.asm
	grep -q "retained result in HL" main.asm
	grep -q "retained arg1 in HL" main.asm
	grep -q "retained arg2 in HL" main.asm
	! grep -q "reason=unmigrated_emitter.*ARRAY" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log