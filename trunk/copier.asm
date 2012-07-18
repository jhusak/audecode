	OPT h-
	org $607 - basic_entry + siotable
tmp = $32
vedge    =$35
buffer   =$408
seclen	= DDEVIC+8
	icl "atari.asm"
siotable
	.by $31 $01 'R' $40
	; +4
	.wo buffer
	; +6
	.by 30
basic_entry
	ldx #6
nsiotab
	lda siotable,x
	sta DDEVIC,x
	pla 
	sta DDEVIC+8,x
	dex
	bpl nsiotab
	jsr SIOV
	sta $d5 ; 0
	sty $d4 ; 1 if ok
	spl:rts

	sei
	jsr nmi ; 1->NMIEN 0->DMACTL
loop:
	lda #$57
	sta chksum
	jsr zero
	ldx #9
nedge
	sta WSYNC
	jsr ledge
	bne nedge
	jsr edge

	ldy #4
	mwa #seclen $7e
	jsr outsector

	; lsb are the same, buffer one page further
	inc $7f 
	ldy seclen
	jsr outsector
	lda chksum
	ldy #0
	jsr outbyte
	ldy #$40
	lda #$22
nmi	sty NMIEN
	sta DMACTL 
	cli
	rts
cedge	bcc edge
ledge	sta WSYNC
edge:
	lda vedge
	; skip next two bytes 
	.byte $2C ; bit Q
zero:
	lda #$1f
	eor #$0f
	sta vedge
	sta WSYNC
	sta AUDC1
	dex
	rts
outsector
	dey
	lda ($7e),y
outbyte
	sta tmp
	clc
	adc chksum
	rol
	adc #0
	sta chksum
	ldx #7
loop2
	asl tmp
	jsr cedge
	bpl loop2
	cpy #0
	bne outsector
	rts
