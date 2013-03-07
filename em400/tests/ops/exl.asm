.program "op/EXL"

; PRE r0 = 0xfafa
; PRE sr = 0b0000001111000000

	uj start
	.res 128
stack:	.res 16

start:
	lw r1, exlp
	rw r1, 96

	lw r1, stack
	rw r1, 97

	exl 23
	hlt 077

exlp:
	hlt 077

.endprog

; XPCT int(rz(6)) : 0

; new process vector

; XPCT bin(sr) : 0b0000001110000000
; XPCT int(r0) : 0

; new ic, exl procedure address

; XPCT int(ic) : 157

; new stack pointer

; XPCT int([97]) : 134

; stack contents

; XPCT int([130]) : 155
; XPCT hex([131]) : 0xfafa
; XPCT bin([132]) : 0b0000001111000000
; XPCT int([133]) : 23

