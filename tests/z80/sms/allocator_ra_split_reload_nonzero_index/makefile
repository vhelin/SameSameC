CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "linear_scan prefer_array_write_value function=allocatorRaSplitReloadNonzeroByte.*phy=A.*displaced_operand=arg2 retained_operand=arg1 reason=shared_next_use" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaSplitReloadNonzeroByte.*ADD.*ARRAY_WRITE arg1" compiler.log
	grep -q "resolve function=allocatorRaSplitReloadNonzeroByte operand=arg1 kind=PHY_A.*phy=A" compiler.log
	grep -q "resolve function=allocatorRaSplitReloadNonzeroByte operand=arg2 kind=STACK_SPILL offset=-13" compiler.log
	grep -q "array_write_index function=allocatorRaSplitReloadNonzeroByte kind=STACK_SPILL.*phy=NONE" compiler.log
	grep -q "array_write_value function=allocatorRaSplitReloadNonzeroByte kind=PHY_A bytes=1.*phy=A" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaSplitReloadNonzeroWord.*SUB.*ARRAY_WRITE arg1" compiler.log
	grep -q "array_write_index function=allocatorRaSplitReloadNonzeroWord kind=STACK_SPILL.*phy=NONE" compiler.log
	grep -q "array_write_value function=allocatorRaSplitReloadNonzeroWord kind=PHY_HL bytes=2.*phy=HL" compiler.log
	grep -q "retained result in A" main.asm
	grep -q "retained arg1 in A" main.asm
	grep -q "retained result in HL" main.asm
	grep -q "retained arg1 in HL" main.asm
	: && ! grep -q "stack offset requested for retained" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log