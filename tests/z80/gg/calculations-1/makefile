ROM = linked.gg
SFILES = interrupts.ssc main.ssc bank_1.ssc bank_2.ssc bank_3.ssc
CLEANFILES = tiles.packbits
include ../../common/calculations_makefile

bank_1.asm: ../../sms/calculations-1/bank_1.ssc tiles.packbits

tiles.packbits: ../../sms/calculations-1/tiles.packbits
	cp $< $@
