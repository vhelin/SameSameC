CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "target_policy function=allocatorRaPolicyAddressNear target=z80 hooks=.*location_materialization,address_materialization.*address_model=target_register_location_and_cache_profile" compiler.log
	test "$$(grep -Ec "target_address_materialization target=(IX|IY|HL) " compiler.log)" -eq 59
	test "$$(grep -c "target=IX kind=GLOBAL offset=999999 mode=global_label folded_offset=0 target_policy=z80" compiler.log)" -eq 6
	test "$$(grep -c "target=IX kind=STACK_LOCAL offset=-5 mode=frame_displacement folded_offset=-5 target_policy=z80" compiler.log)" -eq 5
	test "$$(grep -c "target=IX kind=STACK_LOCAL offset=-147 mode=frame_address folded_offset=0 target_policy=z80" compiler.log)" -eq 2
	test "$$(grep -c "target=IX kind=STACK_LOCAL offset=-147 mode=frame_displacement folded_offset=1 target_policy=z80" compiler.log)" -eq 2
	test "$$(grep -c "target=IX kind=STACK_SPILL offset=-148 mode=frame_address folded_offset=0 target_policy=z80" compiler.log)" -eq 1
	test "$$(grep -c "target=IX kind=STACK_SPILL offset=-148 mode=frame_displacement folded_offset=0 target_policy=z80" compiler.log)" -eq 1
	test "$$(grep -c "target=IY kind=STACK_LOCAL offset=-146 mode=frame_address folded_offset=0 target_policy=z80" compiler.log)" -eq 3
	grep -q "function=allocatorRaPolicyAddressFar emitter=array_read_base_address operand=arg1 kind=STACK_LOCAL offset=-7 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "function=allocatorRaPolicyAddressArrayWord emitter=array_read_base_address operand=arg1 kind=STACK_LOCAL offset=-13 mode=frame_address folded_offset=0 address_target=IY target=z80" compiler.log
	grep -q "target=HL kind=STACK_LOCAL offset=-12 mode=frame_address folded_offset=0 target_policy=z80" compiler.log
	grep -q "target=HL kind=STACK_LOCAL offset=-21 mode=frame_address folded_offset=0 target_policy=z80" compiler.log
	: && ! grep -q "target_address_materialization.*mode=invalid" compiler.log
	: && ! grep -q "has no .* address materialization" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log