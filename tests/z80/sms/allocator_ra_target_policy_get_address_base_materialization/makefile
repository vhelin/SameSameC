CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	test "$$(grep -c "target_specialized_address_materialization.*emitter=get_address_base" compiler.log)" -eq 3
	grep -q "function=allocatorRaPolicyGetAddressBaseGlobal emitter=get_address_base operand=arg1 kind=GLOBAL offset=999999 mode=global_label folded_offset=0 address_target=HL target=z80" compiler.log
	grep -q "function=allocatorRaPolicyGetAddressBaseLocal emitter=get_address_base operand=arg1 kind=STACK_LOCAL offset=-25 mode=frame_address folded_offset=0 address_target=HL target=z80" compiler.log
	grep -q "function=allocatorRaPolicyGetAddressBaseFar emitter=get_address_base operand=arg1 kind=STACK_LOCAL offset=-165 mode=frame_address folded_offset=0 address_target=HL target=z80" compiler.log
	grep -q "get_address_base function=allocatorRaPolicyGetAddressBasePointer kind=PHY_HL memory_pointer_load=no offset=0 phy=HL" compiler.log
	! grep -q "function=allocatorRaPolicyGetAddressBasePointer emitter=get_address_base" compiler.log
	! grep -q "target_specialized_address_materialization.*emitter=get_address_base.*mode=invalid" compiler.log
	! grep -q "_load_location_address_to_hl" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log