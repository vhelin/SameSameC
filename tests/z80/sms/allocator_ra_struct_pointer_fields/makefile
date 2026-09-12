CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "struct_access_indirect function=allocatorRaStructPointerFieldWriteA" compiler.log
	grep -q "struct_access_address function=allocatorRaStructPointerFieldWriteA root=g_allocator_struct_pointer_outer_ptr access=pointer" compiler.log
	grep -q "struct_access_indirect function=allocatorRaStructPointerFieldReadHl" compiler.log
	grep -q "struct_access_address function=allocatorRaStructPointerFieldReadHl root=g_allocator_struct_pointer_outer_ptr access=pointer" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerFieldWriteA.*ADD.*ARRAY_READ arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerFieldWriteA.*ARRAY_READ.*ADD arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaStructPointerFieldWriteA.*ADD.*ARRAY_WRITE result" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerFieldReadHl.*ADD.*ARRAY_READ arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerFieldReadHl.*ARRAY_READ.*ARRAY_READ arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerFieldReadHl.*ARRAY_READ.*XOR arg1" compiler.log
	grep -q "array_read_base function=allocatorRaStructPointerFieldWriteA kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "array_write_base function=allocatorRaStructPointerFieldWriteA kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "array_read_base function=allocatorRaStructPointerFieldReadHl kind=PHY_HL memory_pointer_load=no" compiler.log
	grep -q "register 1 size 2 spill no phy HL" main.asm
	grep -q "retained result in HL" main.asm
	grep -q "retained arg1 in HL" main.asm
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