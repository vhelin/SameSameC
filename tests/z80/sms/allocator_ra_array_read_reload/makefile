CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS = -mz80 -rs 3 -ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

OFILES = main.asm

all: linked.sms
	grep -q "pointer_base_promotion function=allocatorRaReadReloadZero.*reads=2 uses=2 scope=single_block status=complete" compiler.log
	grep -q "pointer_base_promotion function=allocatorRaReadReloadIndex.*reads=3 uses=3 scope=single_block status=complete" compiler.log
	grep -q "pointer_base_promotion function=allocatorRaReadReloadWord.*reads=3 uses=3 scope=single_block status=complete" compiler.log
	grep -q "pointer_base_promotion function=allocatorRaReadReloadSigned.*reads=2 uses=2 scope=single_block status=complete" compiler.log
	test "$$(grep -c 'pointer_base_promotion function=allocatorRaReadReload' compiler.log)" -eq 4
	: && ! grep -q "pointer_base_promotion function=allocatorRaReadReloadReject" compiler.log
	grep -q "reload_graph_transaction function=allocatorRaReadReloadZero.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log
	grep -q "reload_graph_transaction function=allocatorRaReadReloadIndex.*mutations=2 transaction=committed atomic=yes status=complete" compiler.log
	grep -q "reload_graph_transaction function=allocatorRaReadReloadWord.*mutations=2 transaction=committed atomic=yes status=complete" compiler.log
	grep -q "reload_graph_transaction function=allocatorRaReadReloadSigned.*mutations=1 transaction=committed atomic=yes status=complete" compiler.log
	test "$$(grep -c 'reload_insert_emit function=allocatorRaReadReload.*op=ARRAY_READ operand=arg1.*bytes=2 phy=HL operand_id=1' compiler.log)" -eq 6
	grep -q "array_read_index function=allocatorRaReadReloadIndex kind=PHY_A size=1 phy=A" compiler.log
	: && ! grep -q "reload_insert_emit function=allocatorRaReadReloadReject\|stack offset requested for retained" compiler.log
	$(BT) -s main.ssc
	$(CC) -mz80 -rs 3 -ra-all-spill -o all_spill.asm main.ssc > all_spill.log 2>&1 || (cat all_spill.log && false)
	: && ! grep -q 'pointer_base_promotion\|reload_insert_emit' all_spill.log
	$(CC) -mz80 -rs 3 -o disabled.asm main.ssc > disabled.log 2>&1 || (cat disabled.log && false)
	mv disabled.asm default.asm
	$(CC) -mz80 -rs 3 -no-ra -o disabled.asm main.ssc > no_ra.log 2>&1 || (cat no_ra.log && false)
	cmp default.asm disabled.asm
	: && ! grep -q 'register_allocator:' disabled.log no_ra.log

linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)

clean:
	rm -f $(OFILES) all_spill.asm default.asm disabled.asm *~ linked.sms compiler.log all_spill.log disabled.log no_ra.log