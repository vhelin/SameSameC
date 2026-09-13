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
	grep -q "temp_compaction stage=before_first_compress function=allocatorRaTempCompactionDebug temps=2 highest_r=4" compiler.log
	grep -q "temp_compaction stage=after_first_compress function=allocatorRaTempCompactionDebug temps=2 highest_r=1" compiler.log
	grep -q "temp_compaction stage=after_reuse function=allocatorRaTempCompactionDebug temps=1 highest_r=0" compiler.log
	grep -q "temp_compaction stage=after_second_compress function=allocatorRaTempCompactionDebug temps=1 highest_r=0" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaTempCompactionDebug.*producer_tac=6" compiler.log
	grep -q "linear_scan retain_interval function=allocatorRaTempCompactionDebug.*producer_tac=10" compiler.log
	grep -q "spill slot function=allocatorRaTempCompactionDebug r0 offset=-9 size=1" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
