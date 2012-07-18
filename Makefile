all:
	@mads copier.asm -o:copier.bin 
	@xxd -c 1 -ps <copier.bin | awk '{s=s+("0x"$$1)*1}END{print "10 SUM="s" : PLEN="NR}'
	@cat copier.bas
	@xxd -c 2 -ps <copier.bin | tr "[0-9a-f]" "[A-P]" | awk '{if (sw==0) printf("\n%i D. %s",i+20,$$1); else printf(",%s",$$1); if (sw==4) {sw=0; i=i+1; next}; sw=sw+1; }END{printf("\n")}'
pii:
	mads pii.asm -o:pii.xex && open pii.xex
