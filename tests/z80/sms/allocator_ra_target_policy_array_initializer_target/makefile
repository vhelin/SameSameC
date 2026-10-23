CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "target_address_materialization target=HL kind=STACK_LOCAL" compiler.log)" -eq 4
	test "$$(grep -c "target_specialized_address_materialization.*emitter=array_initializer_target" compiler.log)" -eq 4
	grep -q "function=allocatorRaPolicyArrayInitializerNearByte emitter=array_initializer_target operand=target kind=STACK_LOCAL offset=-10 mode=frame_address folded_offset=0 address_target=HL target=z80" compiler.log
	grep -q "function=allocatorRaPolicyArrayInitializerNearWord emitter=array_initializer_target operand=target kind=STACK_LOCAL offset=-17 mode=frame_address folded_offset=0 address_target=HL target=z80" compiler.log
	grep -q "function=allocatorRaPolicyArrayInitializerFarByte emitter=array_initializer_target operand=target kind=STACK_LOCAL offset=-150 mode=frame_address folded_offset=0 address_target=HL target=z80" compiler.log
	grep -q "function=allocatorRaPolicyArrayInitializerFarWord emitter=array_initializer_target operand=target kind=STACK_LOCAL offset=-157 mode=frame_address folded_offset=0 address_target=HL target=z80" compiler.log
	grep -q "array_init_copy function=allocatorRaPolicyArrayInitializerNearByte.*kind=STACK_LOCAL offset=-10 size=1 bytes=6 const_items=6 policy=location_address" compiler.log
	grep -q "array_init_copy function=allocatorRaPolicyArrayInitializerNearWord.*kind=STACK_LOCAL offset=-17 size=2 bytes=12 const_items=6 policy=location_address" compiler.log
	grep -q "array_init_copy function=allocatorRaPolicyArrayInitializerFarByte.*kind=STACK_LOCAL offset=-150 size=1 bytes=6 const_items=6 policy=location_address" compiler.log
	grep -q "array_init_copy function=allocatorRaPolicyArrayInitializerFarWord.*kind=STACK_LOCAL offset=-157 size=2 bytes=12 const_items=6 policy=location_address" compiler.log
	test "$$(grep -c "CALL copy_bytes_bank_" main.asm)" -eq 4
	: && ! grep -q "target_specialized_address_materialization.*emitter=array_initializer_target.*mode=invalid" compiler.log
	: && ! grep -q "_load_location_address_to_hl" compiler.log
	: && ! grep -q "array_init_copy.*kind=PHY" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log