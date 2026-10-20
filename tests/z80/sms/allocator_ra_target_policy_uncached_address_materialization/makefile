CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	: && ! grep -q "target_address_materialization target=IX_UNCACHED" compiler.log
	test "$$(grep -c "target_specialized_address_materialization.*emitter=mul_div_mod_8_arg2" compiler.log)" -eq 3
	grep -q "function=allocatorRaPolicyUncachedMulLocal emitter=mul_div_mod_8_arg2 operand=arg2 kind=STACK_LOCAL offset=-6 mode=frame_displacement folded_offset=-6 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyUncachedDivGlobal emitter=mul_div_mod_8_arg2 operand=arg2 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "function=allocatorRaPolicyUncachedModFar emitter=mul_div_mod_8_arg2 operand=arg2 kind=STACK_LOCAL offset=-148 mode=frame_address folded_offset=0 address_target=IX target=z80" compiler.log
	grep -q "ix_frame_address previous=999999 requested=0 action=materialize setup_cycles=29 setup_bytes=6 " compiler.log
	: && ! grep -q "target_address_materialization target=IX.*mode=invalid" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log