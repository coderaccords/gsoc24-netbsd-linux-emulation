/* $NetBSD: cpu.c,v 1.26 1998/09/22 06:24:26 ross Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: cpu.c,v 1.26 1998/09/22 06:24:26 ross Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>

/* Definition of the driver for autoconfig. */
static int	cpumatch(struct device *, struct cfdata *, void *);
static void	cpuattach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpumatch, cpuattach
};

extern struct cfdriver cpu_cd;

static char *ev4minor[] = {
	"pass 2 or 2.1", "pass 3", 0
}, *lcaminor[] = {
	"",
	"21066 pass 1 or 1.1", "21066 pass 2",
	"21068 pass 1 or 1.1", "21068 pass 2",
	"21066A pass 1", "21068A pass 1", 0
}, *ev5minor[] = {
	"", "pass 2, rev BA or 2.2, rev CA", "pass 2.3, rev DA or EA",
	"pass 3", "pass 3.2", "pass 4", 0
}, *ev45minor[] = {
	"", "pass 1", "pass 1.1", "pass 2", 0
}, *ev56minor[] = {
	"", "pass 1", "pass 2", 0
}, *ev6minor[] = {
	"", "pass 1", 0
}, *pca56minor[] = {
	"", "pass 1", 0
};

struct cputable_struct {
	int	cpu_major_code;
	char	*cpu_major_name;
	char	**cpu_minor_names;
} cpunametable[] = {
	{ PCS_PROC_EV3,		"EV3",		0		},
	{ PCS_PROC_EV4,		"21064",	ev4minor	},
	{ PCS_PROC_SIMULATION,	"Sim",		0		},
	{ PCS_PROC_LCA4,	"LCA",		lcaminor	},
	{ PCS_PROC_EV5,		"21164",	ev5minor	},
	{ PCS_PROC_EV45,	"21064A",	ev45minor	},
	{ PCS_PROC_EV56,	"21164A",	ev56minor	},
	{ PCS_PROC_EV6,		"21264",	ev6minor	},
	{ PCS_PROC_PCA56,	"PCA56",	pca56minor	}
};

static int
cpumatch(parent, cfdata, aux)
	struct device *parent;
	struct cfdata *cfdata;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	/* make sure that we're looking for a CPU. */
	if (strcmp(ma->ma_name, cpu_cd.cd_name) != 0)
		return (0);

	/* XXX CHECK SLOT? */
	/* XXX CHECK PRIMARY? */

	return (1);
}

static void
cpuattach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;
	int i;
	char **s;
        struct pcs *p;
#ifdef DEBUG
	int needcomma;
#endif
	u_int32_t major, minor;

	p = LOCATE_PCS(hwrpb, ma->ma_slot);
	major = PCS_CPU_MAJORTYPE(p);
	minor = PCS_CPU_MINORTYPE(p);

	printf(": ID %d%s, ", ma->ma_slot,
	    ma->ma_slot == hwrpb->rpb_primary_cpu_id ? " (primary)" : "");

	for(i = 0; i < sizeof cpunametable / sizeof cpunametable[0]; ++i) {
		if (cpunametable[i].cpu_major_code == major) {
			printf("%s", cpunametable[i].cpu_major_name);
			s = cpunametable[i].cpu_minor_names;
			for(i = 0; s && s[i]; ++i) {
				if (i == minor) {
					printf(" (%s)\n", s[i]);
					goto recognized;
				}
			}
			printf(" (unknown minor type %d)\n", minor);
			goto recognized;
		}
	}
	printf("UNKNOWN CPU TYPE (%d:%d)", major, minor);

recognized:

#ifdef DEBUG
	/* XXX SHOULD CHECK ARCHITECTURE MASK, TOO */
	if (p->pcs_proc_var != 0) {
		printf("cpu%d: ", dev->dv_unit);

		needcomma = 0;
		if (p->pcs_proc_var & PCS_VAR_VAXFP) {
			printf("VAX FP support");
			needcomma = 1;
		}
		if (p->pcs_proc_var & PCS_VAR_IEEEFP) {
			printf("%sIEEE FP support", needcomma ? ", " : "");
			needcomma = 1;
		}
		if (p->pcs_proc_var & PCS_VAR_PE) {
			printf("%sPrimary Eligible", needcomma ? ", " : "");
			needcomma = 1;
		}
		if (p->pcs_proc_var & PCS_VAR_RESERVED)
			printf("%sreserved bits: 0x%lx", needcomma ? ", " : "",
			    p->pcs_proc_var & PCS_VAR_RESERVED);
		printf("\n");
	}
#endif

	/*
	 * Though we could (should?) attach the LCA cpus' PCI
	 * bus here there is no good reason to do so, and
	 * the bus attachment code is easier to understand
	 * and more compact if done the 'normal' way.
	 */
}
