.prog "cycle/P-over-1"

	lwt r1, -1
	trb r1, 1
	hlt 077
	hlt 077

.finprog

; XPCT int(sr) : 0
; XPCT int(rz[6]) : 0

; XPCT int(ic) : 4
