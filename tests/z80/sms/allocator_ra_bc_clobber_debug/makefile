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
	grep -q "z80_clobber function=allocatorRaBcClobberAdd8.*(ADD).*may_clobber_bc=yes.*clobbers=A|BC|HL|IX|FLAGS" compiler.log
	grep -q "z80_clobber function=allocatorRaBcClobberComplement16.*(COMPLEMENT).*may_clobber_bc=no.*clobbers=A|HL|IX|FLAGS" compiler.log
	grep -q "z80_clobber function=allocatorRaBcClobberSignedAssignment.*(ASSIGNMENT).*may_clobber_bc=yes.*clobbers=A|BC|HL|IX|IY|FLAGS" compiler.log
	grep -q "z80_clobber function=allocatorRaBcClobberJump8.*(JUMP_EQ).*may_clobber_bc=yes.*clobbers=A|BC|HL|IX|FLAGS" compiler.log
	: && ! grep -q "z80_clobber function=allocatorRaBcClobberComplement16.*(COMPLEMENT).*may_clobber_bc=yes" compiler.log


linked.sms: $(OFILES) makefile
	$(LD) $(LDFLAGS) linked.sms $(OFILES)

main.asm: main.ssc
	$(CC) $(CFLAGS) main.asm main.ssc > compiler.log 2>&1 || (cat compiler.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms compiler.log
