CC = $(SAMESAMECVALGRIND) samesamecc
CFLAGS_DEFAULT = -mz80 -rs 3 -o
CFLAGS_NO_RA = -mz80 -rs 3 -no-ra -o
LD = $(SAMESAMECVALGRIND) samesamecl
LDFLAGS = -lsms -v -o
BT = byte_tester

SFILES = main.ssc
IFILES =
OFILES = default.asm no_ra.asm


all: linked.sms linked_no_ra.sms
	cmp -s default.asm no_ra.asm
	cmp -s linked.sms linked_no_ra.sms
	$(BT) -s main.ssc
	: && ! grep -q "register_allocator:" default.log
	: && ! grep -q "register_allocator:" no_ra.log


linked.sms: default.asm makefile
	$(LD) $(LDFLAGS) linked.sms default.asm

linked_no_ra.sms: no_ra.asm makefile
	$(LD) $(LDFLAGS) linked_no_ra.sms no_ra.asm

default.asm: main.ssc
	$(CC) $(CFLAGS_DEFAULT) default.asm main.ssc > default.log 2>&1 || (cat default.log && false)

no_ra.asm: main.ssc
	$(CC) $(CFLAGS_NO_RA) no_ra.asm main.ssc > no_ra.log 2>&1 || (cat no_ra.log && false)


$(OFILES): $(HFILES)


clean:
	rm -f $(OFILES) *~ linked.sms linked_no_ra.sms default.log no_ra.log
