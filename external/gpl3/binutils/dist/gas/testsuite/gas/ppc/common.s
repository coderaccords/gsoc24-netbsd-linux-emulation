	.section ".text"
start:
	and.	3,4,5
	and	3,4,5
	andc	13,14,15
	andc.	16,17,18
	ba	label_abs
	bc	0,1,foo
	bca	4,5,foo_abs
	bcl	2,3,foo
	bcla	10,7,foo_abs
	bctr
	bctrl
	bdza	foo_abs
	bdz	foo
	bdzla	foo_abs
	bdzl	foo
	beq	0,foo
	beqa	2,foo_abs
	beql	1,foo
	beqla	3,foo_abs
	bge	0,foo
	bgea	4,foo_abs
	bgel	2,foo
	bgela	6,foo_abs
	bgt	4,foo
	bgta	6,foo_abs
	bgtl	5,foo
	bgtla	7,foo_abs
	b	label
	bla	label_abs
	ble	0,foo
	blea	4,foo
	blel	2,foo
	blela	6,foo_abs
	bl	label
	blt	0,foo
	blta	2,foo_abs
	bltl	1,foo
	bltla	3,foo_abs
	bne	0,foo
	bnea	2,foo
	bnel	1,foo
	bnela	3,foo_abs
	bng	1,foo
	bnga	5,foo_abs
	bngl	3,foo
	bngla	7,foo_abs
	bnl	1,foo
	bnla	5,foo_abs
	bnll	3,foo
	bnlla	7,foo_abs
	bns	4,foo
	bnsa	6,foo_abs
	bnsl	5,foo
	bnsla	7,foo_abs
	bso	4,foo
	bsoa	6,foo_abs
	bsol	5,foo
	bsola	7,foo_abs
	crand	4,5,6
	crandc	3,4,5
	creqv	7,0,1
	crnand	1,2,3
	crnor	0,1,2
	cror	5,6,7
	crorc	2,3,4
	crxor	6,7,0
	eqv.	10,11,12
	eqv	10,11,12
	fabs.	21,31
	fabs	21,31
	fcmpo	3,10,11
	fcmpu	3,4,5
	fmr.	3,4
	fmr	3,4
	fnabs.	20,30
	fnabs	20,30
	fneg.	3,4
	fneg	3,4
	frsp	6,7
	frsp.	8,9
	lbz	9,0(1)
	lbzu	10,1(1)
	lbzux	20,21,22
	lbzx	3,4,5
	lfd	21,8(1)
	lfdu	22,16(1)
	lfdux	20,21,22
	lfdx	13,14,15
	lfs	19,0(1)
	lfsu	20,4(1)
	lfsux	10,11,12
	lfsx	10,11,12
	lha	15,6(1)
	lhau	16,8(1)
	lhaux	9,10,11
	lhax	9,10,11
	lhbrx	3,4,5
	lhz	13,0(1)
	lhzu	14,2(1)
	lhzux	20,22,24
	lhzx	23,24,25
	mcrf	0,1
	mcrfs	3,4
	mcrxr	3
	mfcr	3
	mfctr	3
	mfdar	5
	mfdsisr	4
	mffs	30
	mffs.	31
	mflr	2
	mfmsr	19
	mfocrf	3,0x80
	mfrtcl	1
	mfrtcu	0
	mfsdr1	6
	mfspr	3,0x80
	mfsrr0	7
	mfsrr1	8
	mfxer	30
	mr.	30,31
	mr	30,31
	mtcr	3
	mtcrf	0x80,3
	mtctr	19
	mtdar	21
	mtdec	24
	mtdsisr	20
	mtfsb0.	3
	mtfsb0	3
	mtfsb1.	3
	mtfsb1	3
	mtfsf	6,10
	mtfsf.	6,11
	mtfsfi	6,0
	mtfsfi.	6,15
	mtlr	18
	mtmsr	10
	mtocrf	0x80,3
	mtrtcl	23
	mtrtcu	22
	mtsdr1	25
	mtspr	0x80,3
	mtsrr0	26
	mtsrr1	27
	mtxer	17
	nand.	28,29,30
	nand	28,29,30
	neg.	3,4
	neg	3,4
	nego	16,17
	nego.	18,19
	nor.	20,21,22
	nor	20,21,22
	not.	20,21
	not	20,21
	or	0,2,4
	or.	12,14,16
	orc	15,16,17
	orc.	18,19,20
	rfi
	stb	11,2(1)
	stbu	12,3(1)
	stbux	13,14,15
	stbx	3,4,5
	stfd	25,32(1)
	stfdu	26,40(1)
	stfdux	0,1,2
	stfdx	29,30,31
	stfs	23,20(1)
	stfsu	24,24(1)
	stfsux	26,27,28
	stfsx	23,24,25
	sth	17,10(1)
	sthbrx	6,7,8
	sthu	18,12(1)
	sthux	21,22,23
	sthx	12,13,14
	xor.	29,30,31
	xor	29,30,31
