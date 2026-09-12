CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = main.asm


all: linked.sms
	grep -q "return_boundary_consumer function=allocatorRaReturnLocationsATemp.*action=defer_to_candidate_policy" compiler.log
	grep -q "linear_scan retain function=allocatorRaReturnLocationsATemp.*8-bit in A.*consumer_tac=.*RETURN_VALUE arg1" compiler.log
	grep -q "return_value_source function=allocatorRaReturnLocationsATemp kind=PHY_A size=1 phy=A" compiler.log
	! grep -q "function=allocatorRaReturnLocationsATemp emitter=return_value_source_address" compiler.log
	! grep -q "function=allocatorRaReturnLocationsAConst emitter=return_value_source_address" compiler.log
	$(BT) -s main.ssc


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log