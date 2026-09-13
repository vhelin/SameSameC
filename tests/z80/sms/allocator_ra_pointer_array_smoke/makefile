CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "migrated_emitter function=allocatorRaPointerArraySmoke.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "linear_scan retain function=allocatorRaPointerArraySmoke.*8-bit in A.*ARRAY_WRITE arg1" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaPointerArraySmoke.*16-bit in HL.*ARRAY_READ arg2" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaPointerArraySmoke.*16-bit in HL.*ARRAY_READ.*XOR arg1" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaPointerArraySmoke.*16-bit in HL.*ARRAY_WRITE arg1" compiler.log
	grep -q "array_read_base function=allocatorRaPointerArraySmoke kind=STACK_LOCAL memory_pointer_load=yes offset=-6" compiler.log
	grep -q "array_read_base function=allocatorRaPointerArraySmoke kind=STACK_LOCAL memory_pointer_load=yes offset=-8" compiler.log
	grep -q "array_write_base function=allocatorRaPointerArraySmoke kind=STACK_LOCAL memory_pointer_load=yes offset=-15" compiler.log
	grep -q "array_write_base function=allocatorRaPointerArraySmoke kind=STACK_LOCAL memory_pointer_load=yes offset=-6" compiler.log
	grep -q "array_write_value function=allocatorRaPointerArraySmoke kind=PHY_A bytes=1" compiler.log
	grep -q "array_write_value function=allocatorRaPointerArraySmoke kind=PHY_HL bytes=2" compiler.log
	grep -q "array_read_index function=allocatorRaPointerArraySmoke kind=PHY_HL" compiler.log
	grep -q "register 0 size 2 spill yes phy NONE" main.asm
	grep -q "register 1 size 1 spill no phy A" main.asm
	grep -q "retained arg1 in A" main.asm
	grep -q "retained arg2 in HL" main.asm
	grep -q "retained result in HL" main.asm
	grep -q "retained arg1 in HL" main.asm
	: && ! grep -q "reason=unmigrated_emitter.*ARRAY" compiler.log
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log