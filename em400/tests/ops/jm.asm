.prog "op/JM"

	lw r1, 14
	sw r1, 19
	jm fin
	hlt 077
fin:	hlt 077


.finprog

; XPCT int(rz[6]) : 0
; XPCT int(sr) : 0

; XPCT int(ic) : 8

