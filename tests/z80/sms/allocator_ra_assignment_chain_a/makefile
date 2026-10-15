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
	grep -q "scalar_copy_promotion function=allocatorRaScalarCopyA .*reads=1 uses=1 scope=function_value status=complete" compiler.log
	grep -q "linear_scan retain function=allocatorRaScalarCopyA.*8-bit in C.*ADD arg2" compiler.log
	grep -q "scalar_copy_promotion function=allocatorRaScalarCopyFallback variable=repeated .*reads=2 " compiler.log
	: && ! grep -E "scalar_copy_promotion function=allocatorRaScalarCopyFallback variable=(escaped|barrier|redefined) " compiler.log
	for name in Bit Word BeforeCall AfterCall AcrossCall; do grep -q "scalar_copy_promotion function=allocatorRaScalar$$name " compiler.log || exit 1; done
	: && ! grep -E "scalar_copy_promotion function=allocatorRaScalar(CallArgument|Narrow) " compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log