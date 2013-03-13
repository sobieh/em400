.program "op/NM"

	lw r1, 0b0000000000000001
	ou r1, 0b0000000000000011
	.data   err, err, ok, err
ok:
	mb blk

	lw r1, 0b1111111100000000
	om r1, 10
	lw r1, 0b0101010101010101
	nm r1, 10

	lw r2, 0
	nm r2, 11

	hlt 077

err:	hlt 077
blk:	.data 1

.endprog

; XPCT int(rz(6)) : 0
; XPCT int(sr) : 1

; XPCT bin([1:10]) : 0b0101010100000000
; XPCT int([1:11]) : 0
; XPCT bin(r0) : 0b1000000000000000