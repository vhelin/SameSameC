CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

OFILES = main.asm

all: linked.sms
	grep -q "struct_access_address function=allocatorRaStructPointerArrayWrite root=g_allocator_struct_pointer_array access=array root_kind=struct_pointer_array" compiler.log
	grep -q "struct_access_address function=allocatorRaStructPointerArrayRead root=g_allocator_struct_pointer_array access=array root_kind=struct_pointer_array" compiler.log
	grep -q "struct_access_indirect function=allocatorRaStructPointerArrayWrite depth=1.*access=pointer" compiler.log
	grep -q "struct_access_indirect function=allocatorRaStructPointerArrayRead depth=1.*access=pointer" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerArrayWrite.*GET_ADDRESS.*ADD arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerArrayWrite.*SHIFT_LEFT.*ADD arg2" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerArrayWrite.*ARRAY_READ.*ADD arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaStructPointerArrayWrite.*ADD.*ARRAY_WRITE result" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerArrayRead.*GET_ADDRESS.*ADD arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerArrayRead.*SHIFT_LEFT.*ADD arg2" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerArrayRead.*ARRAY_READ.*ARRAY_READ arg1" compiler.log
	grep -q "linear_scan retain_interval.*function=allocatorRaStructPointerArrayRead.*ARRAY_READ.*XOR arg1" compiler.log
	grep -q "array_write_base function=allocatorRaStructPointerArrayWrite kind=PHY_HL.*phy=HL" compiler.log
	grep -q "array_read_base function=allocatorRaStructPointerArrayRead kind=PHY_HL.*phy=HL" compiler.log
	: && ! grep -q "reason=unmigrated_emitter.*ARRAY" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc

linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

clean:
	rm -f $(OFILES) linked.sms compiler.log
