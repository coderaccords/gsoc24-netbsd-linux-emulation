/* $NetBSD */

/*-
 * Copyright (c) 2010 Jukka Ruohonen <jruohonen@iki.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_cpu_cstate.c,v 1.1 2010/07/18 09:29:13 jruoho Exp $");

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/once.h>
#include <sys/timetc.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_cpu.h>
#include <dev/acpi/acpi_timer.h>

#include <machine/acpi_machdep.h>

/*
 * This is AML_RESOURCE_GENERIC_REGISTER,
 * included here separately for convenience.
 */
struct acpicpu_reg {
	uint8_t		 reg_desc;
	uint16_t	 reg_reslen;
	uint8_t		 reg_spaceid;
	uint8_t		 reg_bitwidth;
	uint8_t		 reg_bitoffset;
	uint8_t		 reg_accesssize;
	uint64_t	 reg_addr;
} __packed;

static void		 acpicpu_cstate_attach_print(struct acpicpu_softc *);
static ACPI_STATUS	 acpicpu_cstate_cst(struct acpicpu_softc *);
static ACPI_STATUS	 acpicpu_cstate_cst_add(struct acpicpu_softc *,
						ACPI_OBJECT *);
static void		 acpicpu_cstate_cst_bios(void);
static ACPI_STATUS	 acpicpu_cstate_csd(ACPI_HANDLE, struct acpicpu_csd *);
static void		 acpicpu_cstate_fadt(struct acpicpu_softc *);
static void		 acpicpu_cstate_quirks(struct acpicpu_softc *);
static int		 acpicpu_cstate_quirks_piix4(struct pci_attach_args *);
static int		 acpicpu_cstate_latency(struct acpicpu_softc *);
static bool		 acpicpu_cstate_bm_check(void);
static void		 acpicpu_cstate_idle_enter(struct acpicpu_softc *,int);

extern kmutex_t		      acpicpu_mtx;
extern struct acpicpu_softc **acpicpu_sc;
extern int		      acpi_suspended;

void
acpicpu_cstate_attach(device_t self)
{
	struct acpicpu_softc *sc = device_private(self);
	ACPI_STATUS rv;

	/*
	 * Either use the preferred _CST or resort to FADT.
	 */
	rv = acpicpu_cstate_cst(sc);

	switch (rv) {

	case AE_OK:
		sc->sc_flags |= ACPICPU_FLAG_C | ACPICPU_FLAG_C_CST;
		acpicpu_cstate_cst_bios();
		break;

	default:
		sc->sc_flags |= ACPICPU_FLAG_C | ACPICPU_FLAG_C_FADT;
		acpicpu_cstate_fadt(sc);
		break;
	}

	acpicpu_cstate_quirks(sc);
	acpicpu_cstate_attach_print(sc);
}

void
acpicpu_cstate_attach_print(struct acpicpu_softc *sc)
{
	struct acpicpu_cstate *cs;
	struct acpicpu_csd csd;
	const char *method;
	ACPI_STATUS rv;
	int i;

	(void)memset(&csd, 0, sizeof(struct acpicpu_csd));

	rv = acpicpu_cstate_csd(sc->sc_node->ad_handle, &csd);

	if (ACPI_SUCCESS(rv)) {
		aprint_debug_dev(sc->sc_dev, "C%u:  _CSD, "
		    "domain 0x%02x / 0x%02x, type 0x%02x\n",
		    csd.csd_index, csd.csd_domain,
		    csd.csd_ncpu, csd.csd_coord);
	}

	aprint_debug_dev(sc->sc_dev, "Cx: %5s",
	    (sc->sc_flags & ACPICPU_FLAG_C_FADT) != 0 ? "FADT" : "_CST");

	if ((sc->sc_flags & ACPICPU_FLAG_C_BM) != 0)
		aprint_debug(", BM control");

	if ((sc->sc_flags & ACPICPU_FLAG_C_ARB) != 0)
		aprint_debug(", BM arbitration");

	if ((sc->sc_flags & ACPICPU_FLAG_C_C1E) != 0)
		aprint_debug(", C1E");

	if ((sc->sc_flags & ACPICPU_FLAG_C_NOC3) != 0)
		aprint_debug(", C3 disabled (quirk)");

	aprint_debug("\n");

	for (i = 0; i < ACPI_C_STATE_COUNT; i++) {

		cs = &sc->sc_cstate[i];

		if (cs->cs_method == 0)
			continue;

		switch (cs->cs_method) {

		case ACPICPU_C_STATE_HALT:
			method = "HALT";
			break;

		case ACPICPU_C_STATE_FFH:
			method = "FFH";
			break;

		case ACPICPU_C_STATE_SYSIO:
			method = "SYSIO";
			break;

		default:
			panic("NOTREACHED");
		}

		aprint_debug_dev(sc->sc_dev, "C%d: %5s, "
		    "latency %4u, power %4u, addr 0x%06x\n", i, method,
		    cs->cs_latency, cs->cs_power, (uint32_t)cs->cs_addr);
	}
}

int
acpicpu_cstate_detach(device_t self)
{
	static ONCE_DECL(once_detach);

	return RUN_ONCE(&once_detach, acpicpu_md_idle_stop);
}

int
acpicpu_cstate_start(device_t self)
{
	struct acpicpu_softc *sc = device_private(self);
	static ONCE_DECL(once_start);
	static ONCE_DECL(once_save);
	int rv;

	if ((sc->sc_flags & ACPICPU_FLAG_C) == 0)
		return 0;

	/*
	 * Save the existing idle-mechanism and claim the idle_loop(9).
	 * This should be called after all ACPI CPUs have been attached.
	 */
	rv = RUN_ONCE(&once_save, acpicpu_md_idle_init);

	if (rv != 0)
		return rv;

	return RUN_ONCE(&once_start, acpicpu_md_idle_start);
}

bool
acpicpu_cstate_suspend(device_t self)
{

	return true;
}

bool
acpicpu_cstate_resume(device_t self)
{
	static const ACPI_OSD_EXEC_CALLBACK func = acpicpu_cstate_callback;
	struct acpicpu_softc *sc = device_private(self);

	KASSERT((sc->sc_flags & ACPICPU_FLAG_C) != 0);

	if ((sc->sc_flags & ACPICPU_FLAG_C_CST) != 0)
		(void)AcpiOsExecute(OSL_NOTIFY_HANDLER, func, sc->sc_dev);

	return true;
}

void
acpicpu_cstate_callback(void *aux)
{
	struct acpicpu_softc *sc;
	device_t self = aux;

	sc = device_private(self);

	KASSERT((sc->sc_flags & ACPICPU_FLAG_C) != 0);

	if ((sc->sc_flags & ACPICPU_FLAG_C_FADT) != 0) {
		KASSERT((sc->sc_flags & ACPICPU_FLAG_C_CST) == 0);
		return;
	}

	(void)acpicpu_md_idle_stop();
	(void)acpicpu_cstate_cst(sc);
	(void)acpicpu_md_idle_start();
}

static ACPI_STATUS
acpicpu_cstate_cst(struct acpicpu_softc *sc)
{
	ACPI_OBJECT *elm, *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t i, n;
	uint8_t count;

	rv = acpi_eval_struct(sc->sc_node->ad_handle, "_CST", &buf);

	if (ACPI_FAILURE(rv))
		return rv;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	if (obj->Package.Count < 2) {
		rv = AE_LIMIT;
		goto out;
	}

	elm = obj->Package.Elements;

	if (elm[0].Type != ACPI_TYPE_INTEGER) {
		rv = AE_TYPE;
		goto out;
	}

	n = elm[0].Integer.Value;

	if (n != obj->Package.Count - 1) {
		rv = AE_BAD_VALUE;
		goto out;
	}

	if (n > ACPI_C_STATES_MAX) {
		rv = AE_LIMIT;
		goto out;
	}

	(void)memset(sc->sc_cstate, 0,
	    sizeof(*sc->sc_cstate) * ACPI_C_STATE_COUNT);

	for (count = 0, i = 1; i <= n; i++) {

		elm = &obj->Package.Elements[i];
		rv = acpicpu_cstate_cst_add(sc, elm);

		if (ACPI_SUCCESS(rv))
			count++;
	}

	rv = (count != 0) ? AE_OK : AE_NOT_EXIST;

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	if (ACPI_FAILURE(rv))
		aprint_error_dev(sc->sc_dev, "failed to evaluate "
		    "_CST: %s\n", AcpiFormatException(rv));

	return rv;
}

static ACPI_STATUS
acpicpu_cstate_cst_add(struct acpicpu_softc *sc, ACPI_OBJECT *elm)
{
	const struct acpicpu_object *ao = &sc->sc_object;
	struct acpicpu_cstate *cs = sc->sc_cstate;
	struct acpicpu_cstate state;
	struct acpicpu_reg *reg;
	ACPI_STATUS rv = AE_OK;
	ACPI_OBJECT *obj;
	uint32_t type;
	int i;

	(void)memset(&state, 0, sizeof(*cs));

	if (elm->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	if (elm->Package.Count != 4) {
		rv = AE_LIMIT;
		goto out;
	}

	/*
	 * Type.
	 */
	obj = &elm->Package.Elements[1];

	if (obj->Type != ACPI_TYPE_INTEGER) {
		rv = AE_TYPE;
		goto out;
	}

	type = obj->Integer.Value;

	/*
	 * Latency.
	 */
	obj = &elm->Package.Elements[2];

	if (obj->Type != ACPI_TYPE_INTEGER) {
		rv = AE_TYPE;
		goto out;
	}

	state.cs_latency = obj->Integer.Value;

	/*
	 * Power.
	 */
	obj = &elm->Package.Elements[3];

	if (obj->Type != ACPI_TYPE_INTEGER) {
		rv = AE_TYPE;
		goto out;
	}

	state.cs_power = obj->Integer.Value;

	/*
	 * Register.
	 */
	obj = &elm->Package.Elements[0];

	if (obj->Type != ACPI_TYPE_BUFFER) {
		rv = AE_TYPE;
		goto out;
	}

	/*
	 * "When specifically directed by the CPU manufacturer, the
	 *  system firmware may define an interface as functional
	 *  fixed hardware by supplying a special address space
	 *  identifier, FfixedHW (0x7F), in the address space ID
	 *  field for register definitions (ACPI 3.0, p. 46)".
	 */
	reg = (struct acpicpu_reg *)obj->Buffer.Pointer;

	switch (reg->reg_spaceid) {

	case ACPI_ADR_SPACE_SYSTEM_IO:
		state.cs_method = ACPICPU_C_STATE_SYSIO;

		if (reg->reg_addr == 0) {
			rv = AE_AML_ILLEGAL_ADDRESS;
			goto out;
		}

		if (reg->reg_bitwidth != 8) {
			rv = AE_AML_BAD_RESOURCE_LENGTH;
			goto out;
		}

		break;

	case ACPI_ADR_SPACE_FIXED_HARDWARE:
		state.cs_method = ACPICPU_C_STATE_FFH;

		switch (type) {

		case ACPI_STATE_C1:

			if ((sc->sc_flags & ACPICPU_FLAG_C_MWAIT) == 0)
				state.cs_method = ACPICPU_C_STATE_HALT;

			break;

		default:

			if ((sc->sc_flags & ACPICPU_FLAG_C_MWAIT) == 0) {
				rv = AE_AML_BAD_RESOURCE_VALUE;
				goto out;
			}
		}

		break;

	default:
		rv = AE_AML_INVALID_SPACE_ID;
		goto out;
	}

	state.cs_addr = reg->reg_addr;

	CTASSERT(ACPICPU_C_C2_LATENCY_MAX == 100);
	CTASSERT(ACPICPU_C_C3_LATENCY_MAX == 1000);

	CTASSERT(ACPI_STATE_C0 == 0 && ACPI_STATE_C1 == 1);
	CTASSERT(ACPI_STATE_C2 == 2 && ACPI_STATE_C3 == 3);

	switch (type) {

	case ACPI_STATE_C1:
		i = 1;
		break;

	case ACPI_STATE_C2:

		if (state.cs_latency > ACPICPU_C_C2_LATENCY_MAX) {
			rv = AE_BAD_VALUE;
			goto out;
		}

		i = 2;
		break;

	case ACPI_STATE_C3:

		if (state.cs_latency > ACPICPU_C_C3_LATENCY_MAX) {
			rv = AE_BAD_VALUE;
			goto out;
		}

		i = 3;
		break;

	default:
		rv = AE_TYPE;
		goto out;
	}

	/*
	 * Check only that the address is in the mapped space.
	 * Systems are allowed to change it when operating
	 * with _CST (see ACPI 4.0, pp. 94-95). For instance,
	 * the offset of P_LVL3 may change depending on whether
	 * acpiacad(4) is connected or disconnected.
	 */
	if (state.cs_addr > ao->ao_pblkaddr + ao->ao_pblklen) {
		rv = AE_BAD_ADDRESS;
		goto out;
	}

	if (cs[i].cs_method != 0) {
		rv = AE_ALREADY_EXISTS;
		goto out;
	}

#ifndef ACPICPU_ENABLE_C3
	/*
	 * XXX: The local APIC timer (as well as TSC) is typically
	 *	stopped in C3, causing the timer interrupt to fire
	 *	haphazardly, depending on how long the system slept.
	 *	For now, we disable the C3 state unconditionally.
	 */
	if (i == ACPI_STATE_C3) {
		sc->sc_flags |= ACPICPU_FLAG_C_NOC3;
		goto out;
	}
#endif

	cs[i].cs_addr = state.cs_addr;
	cs[i].cs_power = state.cs_power;
	cs[i].cs_latency = state.cs_latency;
	cs[i].cs_method = state.cs_method;

out:
	if (ACPI_FAILURE(rv))
		aprint_verbose_dev(sc->sc_dev,
		    "invalid _CST: %s\n", AcpiFormatException(rv));

	return rv;
}

static void
acpicpu_cstate_cst_bios(void)
{
	const uint8_t val = AcpiGbl_FADT.CstControl;
	const uint32_t addr = AcpiGbl_FADT.SmiCommand;

	if (addr == 0)
		return;

	(void)AcpiOsWritePort(addr, val, 8);
}

static ACPI_STATUS
acpicpu_cstate_csd(ACPI_HANDLE hdl, struct acpicpu_csd *csd)
{
	ACPI_OBJECT *elm, *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	int i, n;

	/*
	 * Query the optional _CSD for heuristics.
	 */
	rv = acpi_eval_struct(hdl, "_CSD", &buf);

	if (ACPI_FAILURE(rv))
		return rv;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	n = obj->Package.Count;

	if (n != 6) {
		rv = AE_LIMIT;
		goto out;
	}

	elm = obj->Package.Elements;

	for (i = 0; i < n; i++) {

		if (elm[i].Type != ACPI_TYPE_INTEGER) {
			rv = AE_TYPE;
			goto out;
		}

		KDASSERT((uint64_t)elm[i].Integer.Value <= UINT32_MAX);
	}

	if (elm[0].Integer.Value != 6 || elm[1].Integer.Value != 0) {
		rv = AE_BAD_DATA;
		goto out;
	}

	csd->csd_domain = elm[2].Integer.Value;
	csd->csd_coord  = elm[3].Integer.Value;
	csd->csd_ncpu   = elm[4].Integer.Value;
	csd->csd_index  = elm[5].Integer.Value;

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	return rv;
}

static void
acpicpu_cstate_fadt(struct acpicpu_softc *sc)
{
	struct acpicpu_cstate *cs = sc->sc_cstate;

	(void)memset(cs, 0, sizeof(*cs) * ACPI_C_STATE_COUNT);

	/*
	 * All x86 processors should support C1 (a.k.a. HALT).
	 */
	if ((AcpiGbl_FADT.Flags & ACPI_FADT_C1_SUPPORTED) != 0)
		cs[ACPI_STATE_C1].cs_method = ACPICPU_C_STATE_HALT;

	if ((acpicpu_md_cpus_running() > 1) &&
	    (AcpiGbl_FADT.Flags & ACPI_FADT_C2_MP_SUPPORTED) == 0)
		return;

	cs[ACPI_STATE_C2].cs_method = ACPICPU_C_STATE_SYSIO;
	cs[ACPI_STATE_C3].cs_method = ACPICPU_C_STATE_SYSIO;

	cs[ACPI_STATE_C2].cs_latency = AcpiGbl_FADT.C2Latency;
	cs[ACPI_STATE_C3].cs_latency = AcpiGbl_FADT.C3Latency;

	cs[ACPI_STATE_C2].cs_addr = sc->sc_object.ao_pblkaddr + 4;
	cs[ACPI_STATE_C3].cs_addr = sc->sc_object.ao_pblkaddr + 5;

	/*
	 * The P_BLK length should always be 6. If it
	 * is not, reduce functionality accordingly.
	 * Sanity check also FADT's latency levels.
	 */
	if (sc->sc_object.ao_pblklen < 5)
		cs[ACPI_STATE_C2].cs_method = 0;

	if (sc->sc_object.ao_pblklen < 6)
		cs[ACPI_STATE_C3].cs_method = 0;

	if (AcpiGbl_FADT.C2Latency > ACPICPU_C_C2_LATENCY_MAX)
		cs[ACPI_STATE_C2].cs_method = 0;

	if (AcpiGbl_FADT.C3Latency > ACPICPU_C_C3_LATENCY_MAX)
		cs[ACPI_STATE_C3].cs_method = 0;

#ifndef ACPICPU_ENABLE_C3
	cs[ACPI_STATE_C3].cs_method = 0;
	sc->sc_flags |= ACPICPU_FLAG_C_NOC3;	/* XXX. */
#endif
}

static void
acpicpu_cstate_quirks(struct acpicpu_softc *sc)
{
	const uint32_t reg = AcpiGbl_FADT.Pm2ControlBlock;
	const uint32_t len = AcpiGbl_FADT.Pm2ControlLength;
	struct pci_attach_args pa;

	/*
	 * Check bus master arbitration.
	 */
	if (reg != 0 && len != 0)
		sc->sc_flags |= ACPICPU_FLAG_C_ARB;
	else {
		/*
		 * Disable C3 entirely if WBINVD is not present.
		 */
		if ((AcpiGbl_FADT.Flags & ACPI_FADT_WBINVD) == 0)
			sc->sc_flags |= ACPICPU_FLAG_C_NOC3;
		else {
			/*
			 * If WBINVD is present, but not functioning
			 * properly according to FADT, flush all CPU
			 * caches before entering the C3 state.
			 */
			if ((AcpiGbl_FADT.Flags & ACPI_FADT_WBINVD_FLUSH) == 0)
				sc->sc_flags &= ~ACPICPU_FLAG_C_BM;
		}
	}

	/*
	 * There are several erratums for PIIX4.
	 */
	if (pci_find_device(&pa, acpicpu_cstate_quirks_piix4) != 0)
		sc->sc_flags |= ACPICPU_FLAG_C_NOC3;

	if ((sc->sc_flags & ACPICPU_FLAG_C_NOC3) != 0)
		sc->sc_cstate[ACPI_STATE_C3].cs_method = 0;
}

static int
acpicpu_cstate_quirks_piix4(struct pci_attach_args *pa)
{

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return 0;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82371AB_ISA ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82440MX_PMC)
		return 1;

	return 0;
}

static int
acpicpu_cstate_latency(struct acpicpu_softc *sc)
{
	static const uint32_t cs_factor = 3;
	struct acpicpu_cstate *cs;
	int i;

	for (i = ACPI_STATE_C3; i > 0; i--) {

		cs = &sc->sc_cstate[i];

		if (__predict_false(cs->cs_method == 0))
			continue;

		/*
		 * Choose a state if we have previously slept
		 * longer than the worst case latency of the
		 * state times an arbitrary multiplier.
		 */
		if (sc->sc_sleep > cs->cs_latency * cs_factor)
			return i;
	}

	return ACPI_STATE_C1;
}

/*
 * The main idle loop.
 */
void
acpicpu_cstate_idle(void)
{
        struct cpu_info *ci = curcpu();
	struct acpicpu_softc *sc;
	int state;

	if (__predict_false(ci->ci_want_resched) != 0)
		return;

	KASSERT(acpicpu_sc != NULL);
	KASSERT(ci->ci_cpuid < maxcpus);
	KASSERT(ci->ci_ilevel == IPL_NONE);

	if (__predict_false(acpi_suspended != 0)) {
		acpicpu_md_idle_enter(0, 0);
		return;
	}

	sc = acpicpu_sc[ci->ci_cpuid];

	/*
	 * If all CPUs do not have an ACPI counterpart,
	 * the softc may be NULL. In this case use C1.
	 */
	if (__predict_false(sc == NULL)) {
		acpicpu_md_idle_enter(0, 0);
		return;
	}

	acpi_md_OsDisableInterrupt();
	state = acpicpu_cstate_latency(sc);

	/*
	 * Check for bus master activity. Note that
	 * particularly usb(4) causes high activity,
	 * which may prevent the use of C3 states.
	 */
	if (acpicpu_cstate_bm_check() != false) {

		state--;

		if (__predict_false(sc->sc_cstate[state].cs_method == 0))
			state = ACPI_STATE_C1;
	}

	KASSERT(state != ACPI_STATE_C0);

	if (state != ACPI_STATE_C3) {
		acpicpu_cstate_idle_enter(sc, state);
		return;
	}

	/*
	 * On all recent (Intel) CPUs caches are shared
	 * by CPUs and bus master control is required to
	 * keep these coherent while in C3. Flushing the
	 * CPU caches is only the last resort.
	 */
	if ((sc->sc_flags & ACPICPU_FLAG_C_BM) == 0)
		ACPI_FLUSH_CPU_CACHE();

	/*
	 * Some chipsets may not return back to C0
	 * from C3 if bus master wake is not enabled.
	 */
	if ((sc->sc_flags & ACPICPU_FLAG_C_BM) != 0)
		(void)AcpiWriteBitRegister(ACPI_BITREG_BUS_MASTER_RLD, 1);

	/*
	 * It may be necessary to disable bus master arbitration
	 * to ensure that bus master cycles do not occur while
	 * sleeping in C3 (see ACPI 4.0, section 8.1.4).
	 */
	if ((sc->sc_flags & ACPICPU_FLAG_C_ARB) != 0)
		(void)AcpiWriteBitRegister(ACPI_BITREG_ARB_DISABLE, 1);

	acpicpu_cstate_idle_enter(sc, state);

	/*
	 * Disable bus master wake and re-enable the arbiter.
	 */
	if ((sc->sc_flags & ACPICPU_FLAG_C_BM) != 0)
		(void)AcpiWriteBitRegister(ACPI_BITREG_BUS_MASTER_RLD, 0);

	if ((sc->sc_flags & ACPICPU_FLAG_C_ARB) != 0)
		(void)AcpiWriteBitRegister(ACPI_BITREG_ARB_DISABLE, 0);
}

static void
acpicpu_cstate_idle_enter(struct acpicpu_softc *sc, int state)
{
	struct acpicpu_cstate *cs = &sc->sc_cstate[state];
	uint32_t end, start, val;

	start = acpitimer_read_safe(NULL);

	switch (cs->cs_method) {

	case ACPICPU_C_STATE_FFH:
	case ACPICPU_C_STATE_HALT:
		acpicpu_md_idle_enter(cs->cs_method, state);
		break;

	case ACPICPU_C_STATE_SYSIO:
		(void)AcpiOsReadPort(cs->cs_addr, &val, 8);
		break;

	default:
		acpicpu_md_idle_enter(0, 0);
		break;
	}

	cs->cs_stat++;

	end = acpitimer_read_safe(NULL);
	sc->sc_sleep = hztoms(acpitimer_delta(end, start)) * 1000;

	acpi_md_OsEnableInterrupt();
}

static bool
acpicpu_cstate_bm_check(void)
{
	uint32_t val = 0;
	ACPI_STATUS rv;

	rv = AcpiReadBitRegister(ACPI_BITREG_BUS_MASTER_STATUS, &val);

	if (ACPI_FAILURE(rv) || val == 0)
		return false;

	(void)AcpiWriteBitRegister(ACPI_BITREG_BUS_MASTER_STATUS, 1);

	return true;
}
