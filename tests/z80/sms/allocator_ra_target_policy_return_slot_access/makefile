CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyReturnSlotAccess target=z80 hooks=.*active_slots.*return_slot_access.*return_slot_model=byte_offsets" compiler.log
	grep -q "target_return_slot_access function=allocatorRaPolicyReturnSignedByte role=callee_store size=1 target=z80 low_offset=-4 high_offset=0" compiler.log
	grep -q "target_return_slot_access function=allocatorRaPolicyReturnUnsignedByte role=callee_store size=1 target=z80 low_offset=-4 high_offset=0" compiler.log
	grep -q "target_return_slot_access function=allocatorRaPolicyReturnWord role=callee_store size=2 target=z80 low_offset=-5 high_offset=-4" compiler.log
	grep -q "target_return_slot_access function=allocatorRaPolicyReturnSlotAccess callee=allocatorRaPolicyReturnSignedByte role=caller_load size=1 target=z80 low_offset=-4 high_offset=0" compiler.log
	grep -q "target_return_slot_access function=allocatorRaPolicyReturnSlotAccess callee=allocatorRaPolicyReturnUnsignedByte role=caller_load size=1 target=z80 low_offset=-4 high_offset=0" compiler.log
	grep -q "target_return_slot_access function=allocatorRaPolicyReturnSlotAccess callee=allocatorRaPolicyReturnWord role=caller_load size=2 target=z80 low_offset=-5 high_offset=-4" compiler.log
	! grep -q "Target policy cannot locate" compiler.log
	test "$$(grep -c "return value -> L" main.asm)" -eq 2
	test "$$(grep -c "return value -> HL" main.asm)" -eq 1
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
