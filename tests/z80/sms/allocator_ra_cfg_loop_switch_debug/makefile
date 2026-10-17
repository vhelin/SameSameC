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
	grep -q "cfg_edge function=allocatorRaCfgLoopDebug.*kind=branch_true" compiler.log
	grep -q "cfg_edge function=allocatorRaCfgLoopDebug.*kind=branch_false" compiler.log
	grep -q "cfg_edge function=allocatorRaCfgLoopDebug from_block=2 to_block=1 kind=jump" compiler.log
	grep -q "liveness function=allocatorRaCfgLoopDebug block=1 use={} def={r0} live_in={} live_out={}" compiler.log
	grep -q "cfg_edge function=allocatorRaCfgSwitchDebug.*kind=branch_true" compiler.log
	grep -q "cfg_edge function=allocatorRaCfgSwitchDebug.*kind=branch_false" compiler.log
	grep -q "cfg_edge function=allocatorRaCfgSwitchDebug.*kind=fallthrough" compiler.log
	grep -q "liveness function=allocatorRaCfgSwitchDebug block=1 use={} def={r0} live_in={} live_out={}" compiler.log
	for flags in "" "-ra" "-ra-all-spill"; do \
		if $(CC) -mz80 -rs 3 $$flags -o invalid_continue.asm invalid_continue.ssc > invalid_continue.log 2>&1; then \
			echo "Switch-only continue was unexpectedly accepted"; exit 1; \
		fi; \
		grep -q "continue does nothing here, nothing to continue" invalid_continue.log || { cat invalid_continue.log; exit 1; }; \
	done


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log invalid_continue.asm invalid_continue.log
