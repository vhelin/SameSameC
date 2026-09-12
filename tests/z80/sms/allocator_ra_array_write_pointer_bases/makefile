CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "migrated_emitter function=allocatorRaArrayWritePointerBaseA.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "migrated_emitter function=allocatorRaArrayWritePointerBaseHl.*ARRAY_WRITE.*barrier=removed" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWritePointerBaseA.*producer_tac=.*ADD.*consumer_tac=.*ARRAY_WRITE.*arg1" compiler.log
	grep -q "linear_scan retain.*function=allocatorRaArrayWritePointerBaseHl.*producer_tac=.*XOR.*consumer_tac=.*ARRAY_WRITE.*arg1" compiler.log
	grep -q "resolve function=allocatorRaArrayWritePointerBaseA operand=result kind=STACK_LOCAL" compiler.log
	grep -q "resolve function=allocatorRaArrayWritePointerBaseHl operand=result kind=STACK_LOCAL" compiler.log
	grep -q "array_write_base function=allocatorRaArrayWritePointerBaseA kind=STACK_LOCAL memory_pointer_load=yes" compiler.log
	grep -q "array_write_base function=allocatorRaArrayWritePointerBaseHl kind=STACK_LOCAL memory_pointer_load=yes" compiler.log
	grep -q "function=allocatorRaArrayWritePointerBaseA emitter=array_write_base_address operand=result kind=STACK_LOCAL offset=-6 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaArrayWritePointerBaseHl emitter=array_write_base_address operand=result kind=STACK_LOCAL offset=-7 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	! grep -q "reason=unmigrated_emitter.*ARRAY_WRITE" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	grep -q "retained arg1 in A" main.asm
	grep -q "retained arg1 in HL" main.asm
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
