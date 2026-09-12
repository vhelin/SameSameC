CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "resolve function=allocatorRaArrayInitCopyLocationA operand=array_init_target kind=STACK_LOCAL" compiler.log
	grep -q "resolve function=allocatorRaArrayInitCopyLocationHl operand=array_init_target kind=STACK_LOCAL" compiler.log
	grep -q "array_init_copy function=allocatorRaArrayInitCopyLocationA variable=.*values.*kind=STACK_LOCAL.*bytes=6.*policy=location_address" compiler.log
	grep -q "array_init_copy function=allocatorRaArrayInitCopyLocationHl variable=.*values.*kind=STACK_LOCAL.*bytes=12.*policy=location_address" compiler.log
	test "$$(grep -c "target_specialized_address_materialization.*emitter=array_initializer_target" compiler.log)" -eq 2
	grep -q "function=allocatorRaArrayInitCopyLocationA emitter=array_initializer_target operand=target kind=STACK_LOCAL offset=-12 mode=frame_address folded_offset=0 address_target=HL target=z80" compiler.log
	grep -q "function=allocatorRaArrayInitCopyLocationHl emitter=array_initializer_target operand=target kind=STACK_LOCAL offset=-21 mode=frame_address folded_offset=0 address_target=HL target=z80" compiler.log
	grep -q "CALL copy_bytes_bank_" main.asm
	! grep -q "array_init_copy.*kind=PHY" compiler.log
	! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log