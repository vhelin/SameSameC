CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	$(BT) -s main.ssc
	grep -q "linear_scan bc_transparent_candidate function=allocatorRaBcRetentionHlConflict" compiler.log
	grep -q "linear_scan retain function=allocatorRaBcRetentionHlConflict.*r0 16-bit in HL" compiler.log
	grep -q "linear_scan overlap_check function=allocatorRaBcRetentionHlConflict.*candidate=BC.*candidate_units=B|C.*active_HL=yes.*active_B=no.*active_C=no.*conflict=no" compiler.log
	grep -q "linear_scan alternate function=allocatorRaBcRetentionHlConflict.*primary=HL alternate=BC" compiler.log
	grep -q "linear_scan retain function=allocatorRaBcRetentionHlConflict.*r1 16-bit in BC" compiler.log
	grep -q "resolve function=allocatorRaBcRetentionHlConflict operand=result kind=PHY_BC" compiler.log
	grep -q "function=allocatorRaBcRetentionHlConflict emitter=complement_16bit_arg1_address operand=arg1 kind=STACK_LOCAL offset=-11 mode=frame_displacement folded_offset=-11 address_target=IX target=z80" compiler.log
	: && ! grep -q "function=allocatorRaBcRetentionHlConflict emitter=complement_16bit_result_address" compiler.log
	grep -q "register 1 size 2 spill no phy BC" main.asm
	grep -q "retained result in BC" main.asm
	grep -q "retained arg2 in BC" main.asm


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
