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
	grep -q "linear_scan spill function=allocatorRaLinearScanFartherNextUseHl" compiler.log
	grep -q "reason=farther_next_use" compiler.log
	grep -q "linear_scan competing_candidate function=allocatorRaLinearScanFartherNextUseA" compiler.log
	grep -q "linear_scan competing_candidate function=allocatorRaLinearScanFartherNextUseHl" compiler.log
	grep -q "linear_scan alternate function=allocatorRaLinearScanFartherNextUseA.*primary=A alternate=C reason=active_register_conflict" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaLinearScanFartherNextUseA.*8-bit in C" compiler.log
	grep -q "linear_scan alternate function=allocatorRaLinearScanFartherNextUseBoundaryA.*primary=A alternate=C reason=clobber_between" compiler.log
	grep -q "linear_scan retain function=allocatorRaLinearScanFartherNextUseBoundaryA.*8-bit in C.*ADD arg1" compiler.log
	grep -q "linear_scan skip function=allocatorRaLinearScanFartherNextUseBoundaryHl.*reason=clobber_between" compiler.log
	! grep -q "linear_scan spill function=allocatorRaLinearScanFartherNextUseA.*reason=farther_next_use" compiler.log
	! grep -q "linear_scan spill function=allocatorRaLinearScanFartherNextUseBoundaryA.*reason=farther_next_use" compiler.log
	! grep -q "linear_scan spill function=allocatorRaLinearScanFartherNextUseBoundaryHl.*reason=farther_next_use" compiler.log
	grep -q "register 0 size 1 spill no phy A" main.asm
	grep -q "register 1 size 1 spill yes phy NONE" main.asm
	grep -q "register 0 size 1 spill no phy C" main.asm
	grep -q "register 1 size 2 spill no phy HL" main.asm


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log