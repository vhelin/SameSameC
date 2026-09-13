CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -Ec "target_specialized_address_materialization.*emitter=(get_address_index|mul_div_mod_16_arg2)" compiler.log)" -eq 5
	grep -q "function=allocatorRaPolicySpecializedGetLocal emitter=get_address_index operand=arg2 kind=STACK_LOCAL offset=-7 mode=frame_displacement folded_offset=-7 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicySpecializedGetGlobal emitter=get_address_index operand=arg2 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicySpecializedMulLocal emitter=mul_div_mod_16_arg2 operand=arg2 kind=STACK_LOCAL offset=-9 mode=frame_displacement folded_offset=-9 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicySpecializedDivGlobal emitter=mul_div_mod_16_arg2 operand=arg2 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicySpecializedModFar emitter=mul_div_mod_16_arg2 operand=arg2 kind=STACK_LOCAL offset=-153 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	: && ! grep -q "target_specialized_address_materialization.*mode=invalid" compiler.log
	: && ! grep -q "has no IX address materialization" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log