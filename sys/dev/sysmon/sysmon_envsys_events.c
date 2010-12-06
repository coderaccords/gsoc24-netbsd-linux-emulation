/* $NetBSD: sysmon_envsys_events.c,v 1.94 2010/12/06 23:26:44 pgoyette Exp $ */

/*-
 * Copyright (c) 2007, 2008 Juan Romero Pardines.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * sysmon_envsys(9) events framework.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon_envsys_events.c,v 1.94 2010/12/06 23:26:44 pgoyette Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/kmem.h>
#include <sys/callout.h>

/* #define ENVSYS_DEBUG */
/* #define ENVSYS_OBJECTS_DEBUG */

#include <dev/sysmon/sysmonvar.h>
#include <dev/sysmon/sysmon_envsysvar.h>

struct sme_sensor_event {
	int		state;
	int		event;
};

static const struct sme_sensor_event sme_sensor_event[] = {
	{ ENVSYS_SVALID,			PENVSYS_EVENT_NORMAL },
	{ ENVSYS_SCRITOVER, 			PENVSYS_EVENT_CRITOVER },
	{ ENVSYS_SCRITUNDER, 			PENVSYS_EVENT_CRITUNDER },
	{ ENVSYS_SWARNOVER, 			PENVSYS_EVENT_WARNOVER },
	{ ENVSYS_SWARNUNDER,			PENVSYS_EVENT_WARNUNDER },
	{ ENVSYS_BATTERY_CAPACITY_NORMAL,	PENVSYS_EVENT_NORMAL },
	{ ENVSYS_BATTERY_CAPACITY_WARNING,	PENVSYS_EVENT_BATT_WARN },
	{ ENVSYS_BATTERY_CAPACITY_CRITICAL,	PENVSYS_EVENT_BATT_CRIT },
	{ ENVSYS_BATTERY_CAPACITY_HIGH,		PENVSYS_EVENT_BATT_HIGH },
	{ ENVSYS_BATTERY_CAPACITY_MAX,		PENVSYS_EVENT_BATT_MAX },
	{ -1, 					-1 }
};

static bool sysmon_low_power;

#define SME_EVTIMO	(SME_EVENTS_DEFTIMEOUT * hz)

static bool sme_event_check_low_power(void);
static bool sme_battery_check(void);
static bool sme_battery_critical(envsys_data_t *);
static bool sme_acadapter_check(void);

/*
 * sme_event_register:
 *
 * 	+ Registers a new sysmon envsys event or updates any event
 * 	  already in the queue.
 */
int
sme_event_register(prop_dictionary_t sdict, envsys_data_t *edata,
		   struct sysmon_envsys *sme, sysmon_envsys_lim_t *lims,
		   uint32_t props, int crittype, int powertype)
{
	sme_event_t *see = NULL, *osee = NULL;
	prop_object_t obj;
	int error = 0;
	const char *objkey;

	KASSERT(sdict != NULL);
	KASSERT(edata != NULL);
	KASSERT(sme != NULL);
	KASSERT(lims != NULL);

	/*
	 * Some validation first for limit-checking events
	 *
	 * 1. Limits are not permitted if the units is ENVSYS_INDICATOR.
	 *
	 * 2. Capacity limits are permitted only if the sensor has the
	 *    ENVSYS_FPERCENT flag set and value_max is set.
	 *
	 * 3. It is not permissible for both capacity and value limits
	 *    to coexist.
	 *
	 * Note that it permissible for a sensor to have value limits
	 * even if its ENVSYS_FPERCENT flag and value_max are set.
	 */

	DPRINTF(("%s: units %d props 0x%04x upropset 0x%04x max_val %d"
		" edata-flags 0x%04x\n", __func__, edata->units, props,
		edata->upropset, edata->value_max, edata->flags));

	if (props && edata->units == ENVSYS_INDICATOR)
		return ENOTSUP;

	if ((props & PROP_CAP_LIMITS) &&
	    ((edata->value_max == 0) ||
	     !(edata->flags & ENVSYS_FPERCENT) ||
	     (props & PROP_VAL_LIMITS) ||
	     (edata->upropset & PROP_VAL_LIMITS)))
		props = 0;

	if ((props & PROP_VAL_LIMITS) && (edata->upropset & PROP_CAP_LIMITS))
		props = 0;

	/* 
	 * check if the event is already on the list and return
	 * EEXIST if value provided hasn't been changed.
	 */
	mutex_enter(&sme->sme_mtx);
	LIST_FOREACH(osee, &sme->sme_events_list, see_list) {
		if (strcmp(edata->desc, osee->see_pes.pes_sensname) != 0)
			continue;
		if (crittype != osee->see_type)
			continue;

		/*
		 * We found an existing event for this sensor.  Make 
		 * sure it references the correct edata
		 */
		KASSERT(edata == osee->see_edata);

		DPRINTF(("%s: dev %s sensor %s: event type %d exists\n",
		    __func__, sme->sme_name, edata->desc, crittype));

		see = osee;
		if (props & (PROP_CRITMAX | PROP_BATTMAX)) {
			if (lims->sel_critmax == edata->limits.sel_critmax) {
				DPRINTF(("%s: type=%d (critmax exists)\n",
				    __func__, crittype));
				error = EEXIST;
				props &= ~(PROP_CRITMAX | PROP_BATTMAX);
			}
		}
		if (props & (PROP_WARNMAX | PROP_BATTHIGH)) {
			if (lims->sel_warnmax == edata->limits.sel_warnmax) {
				DPRINTF(("%s: warnmax exists\n", __func__));
				error = EEXIST;
				props &= ~(PROP_WARNMAX | PROP_BATTHIGH);
			}
		}
		if (props & (PROP_WARNMIN | PROP_BATTWARN)) {
			if (lims->sel_warnmin == edata->limits.sel_warnmin) {
				DPRINTF(("%s: warnmin exists\n", __func__));
				error = EEXIST;
				props &= ~(PROP_WARNMIN | PROP_BATTWARN);
			}
		}
		if (props & (PROP_CRITMIN | PROP_BATTCAP)) {
			if (lims->sel_critmin == edata->limits.sel_critmin) {
				DPRINTF(("%s: critmin exists\n", __func__));
				error = EEXIST;
				props &= ~(PROP_CRITMIN | PROP_BATTCAP);
			}
		}
		break;
	}
	if (see == NULL) {
		/*
		 * New event requested - allocate a sysmon_envsys event.
		 */
		see = kmem_zalloc(sizeof(*see), KM_SLEEP);
		if (see == NULL)
			return ENOMEM;

		DPRINTF(("%s: dev %s sensor %s: new event\n",
		    __func__, sme->sme_name, edata->desc));

		see->see_type = crittype;
		see->see_sme = sme;
		see->see_edata = edata;

		/* Initialize sensor type and previously-sent state */

		see->see_pes.pes_type = powertype;

		switch (crittype) {
		case PENVSYS_EVENT_LIMITS:
			see->see_evsent = ENVSYS_SVALID;
			break;
		case PENVSYS_EVENT_CAPACITY:
			see->see_evsent = ENVSYS_BATTERY_CAPACITY_NORMAL;
			break;
		case PENVSYS_EVENT_STATE_CHANGED:
			if (edata->units == ENVSYS_BATTERY_CAPACITY)
				see->see_evsent = ENVSYS_BATTERY_CAPACITY_NORMAL;
			else if (edata->units == ENVSYS_DRIVE)
				see->see_evsent = ENVSYS_DRIVE_EMPTY;
			else
				panic("%s: bad units for "
				      "PENVSYS_EVENT_STATE_CHANGED", __func__);
			break;
		case PENVSYS_EVENT_CRITICAL:
		default:
			see->see_evsent = 0;
			break;
		}

		(void)strlcpy(see->see_pes.pes_dvname, sme->sme_name,
		    sizeof(see->see_pes.pes_dvname));
		(void)strlcpy(see->see_pes.pes_sensname, edata->desc,
		    sizeof(see->see_pes.pes_sensname));
	}

	/*
	 * Limit operation requested.
	 */
#define	LIMIT_OP(k, l, p)						\
	if (props & p) {						\
		objkey = k;						\
		obj = prop_dictionary_get(sdict, objkey);		\
		if (obj != NULL &&					\
		    prop_object_type(obj) != PROP_TYPE_NUMBER) {	\
			DPRINTF(("%s: (%s) %s object no TYPE_NUMBER\n",	\
			    __func__, sme->sme_name, objkey));		\
			error = ENOTSUP;				\
		} else {						\
			edata->limits.l = lims->l;			\
			error = sme_sensor_upint32(sdict, objkey,lims->l); \
			DPRINTF(("%s: (%s) event [sensor=%s type=%d] "	\
			    "(%s updated)\n", __func__, sme->sme_name,	\
			    edata->desc, crittype, objkey));		\
		}							\
		if (error && error != EEXIST)				\
			goto out;					\
		edata->upropset |= p;					\
	}

	/* Value-based limits */
	LIMIT_OP("critical-max", sel_critmax, PROP_CRITMAX);
	LIMIT_OP("warning-max",  sel_warnmax, PROP_WARNMAX);
	LIMIT_OP("warning-min",  sel_warnmin, PROP_WARNMIN);
	LIMIT_OP("critical-min", sel_critmin, PROP_CRITMIN);

	/* %Capacity-based limits */
	LIMIT_OP("maximum-capacity",  sel_critmax,  PROP_BATTMAX);
	LIMIT_OP("high-capacity",     sel_warnmax,  PROP_BATTHIGH);
	LIMIT_OP("warning-capacity",  sel_warnmin,  PROP_BATTWARN);
	LIMIT_OP("critical-capacity", sel_critmin,  PROP_BATTCAP);

#undef LIMIT_OP

	if (props & PROP_DRIVER_LIMITS)
		edata->upropset |= PROP_DRIVER_LIMITS;
	else
		edata->upropset &= ~PROP_DRIVER_LIMITS;

	DPRINTF(("%s: (%s) event registered (sensor=%s snum=%d type=%d "
	    "critmin=%" PRIu32 " warnmin=%" PRIu32 " warnmax=%" PRIu32
	    " critmax=%" PRIu32 " props 0x%04x)\n", __func__,
	    see->see_sme->sme_name, see->see_pes.pes_sensname,
	    edata->sensor, see->see_type, edata->limits.sel_critmin,
	    edata->limits.sel_warnmin, edata->limits.sel_warnmax,
	    edata->limits.sel_critmax, edata->upropset));
	/*
	 * Initialize the events framework if it wasn't initialized before.
	 */
	if ((sme->sme_flags & SME_CALLOUT_INITIALIZED) == 0)
		error = sme_events_init(sme);

	/*
	 * If driver requested notification, advise it of new
	 * limit values
	 */
	if (sme->sme_set_limits)
		(*sme->sme_set_limits)(sme, edata, &(edata->limits),
					&(edata->upropset));

out:
	if ((error == 0 || error == EEXIST) && osee == NULL)
		LIST_INSERT_HEAD(&sme->sme_events_list, see, see_list);

	mutex_exit(&sme->sme_mtx);

	return error;
}

/*
 * sme_event_unregister_all:
 *
 * 	+ Unregisters all events associated with a sysmon envsys device.
 */
void
sme_event_unregister_all(struct sysmon_envsys *sme)
{
	sme_event_t *see;
	int evcounter = 0;

	KASSERT(sme != NULL);

	mutex_enter(&sme->sme_mtx);
	LIST_FOREACH(see, &sme->sme_events_list, see_list) {
		while (see->see_flags & SEE_EVENT_WORKING)
			cv_wait(&sme->sme_condvar, &sme->sme_mtx);

		if (strcmp(see->see_pes.pes_dvname, sme->sme_name) == 0)
			evcounter++;
	}

	DPRINTF(("%s: total events %d (%s)\n", __func__,
	    evcounter, sme->sme_name));

	while ((see = LIST_FIRST(&sme->sme_events_list))) {
		if (evcounter == 0)
			break;

		if (strcmp(see->see_pes.pes_dvname, sme->sme_name) == 0) {
			LIST_REMOVE(see, see_list);
			DPRINTF(("%s: event %s %d removed (%s)\n", __func__,
			    see->see_pes.pes_sensname, see->see_type,
			    sme->sme_name));
			kmem_free(see, sizeof(*see));
			evcounter--;
		}
	}

	if (LIST_EMPTY(&sme->sme_events_list))
		if (sme->sme_flags & SME_CALLOUT_INITIALIZED)
			sme_events_destroy(sme);
	mutex_exit(&sme->sme_mtx);
}

/*
 * sme_event_unregister:
 *
 * 	+ Unregisters an event from the specified sysmon envsys device.
 */
int
sme_event_unregister(struct sysmon_envsys *sme, const char *sensor, int type)
{
	sme_event_t *see;
	bool found = false;

	KASSERT(sensor != NULL);

	mutex_enter(&sme->sme_mtx);
	LIST_FOREACH(see, &sme->sme_events_list, see_list) {
		if (strcmp(see->see_pes.pes_sensname, sensor) == 0) {
			if (see->see_type == type) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		mutex_exit(&sme->sme_mtx);
		return EINVAL;
	}

	/*
	 * Wait for the event to finish its work, remove from the list
	 * and release resouces.
	 */
	while (see->see_flags & SEE_EVENT_WORKING)
		cv_wait(&sme->sme_condvar, &sme->sme_mtx);

	DPRINTF(("%s: removed dev=%s sensor=%s type=%d\n",
	    __func__, see->see_pes.pes_dvname, sensor, type));
	LIST_REMOVE(see, see_list);
	/*
	 * So the events list is empty, we'll do the following:
	 *
	 * 	- stop and destroy the callout.
	 * 	- destroy the workqueue.
	 */
	if (LIST_EMPTY(&sme->sme_events_list))
		sme_events_destroy(sme);
	mutex_exit(&sme->sme_mtx);

	kmem_free(see, sizeof(*see));
	return 0;
}

/*
 * sme_event_drvadd:
 *
 * 	+ Registers a new event for a device that had enabled any of
 * 	  the monitoring flags in the driver.
 */
void
sme_event_drvadd(void *arg)
{
	sme_event_drv_t *sed_t = arg;
	sysmon_envsys_lim_t lims;
	uint32_t props;
	int error = 0;

	KASSERT(sed_t != NULL);

#define SEE_REGEVENT(a, b, c)						\
do {									\
	if (sed_t->sed_edata->flags & (a)) {				\
		char str[ENVSYS_DESCLEN] = "monitoring-state-";		\
									\
		error = sme_event_register(sed_t->sed_sdict,		\
				      sed_t->sed_edata,			\
				      sed_t->sed_sme,			\
				      &lims, props,			\
				      (b),				\
				      sed_t->sed_powertype);		\
		if (error && error != EEXIST)				\
			printf("%s: failed to add event! "		\
			    "error=%d sensor=%s event=%s\n",		\
			    __func__, error,				\
			    sed_t->sed_edata->desc, (c));		\
		else {							\
			(void)strlcat(str, (c), sizeof(str));		\
			prop_dictionary_set_bool(sed_t->sed_sdict,	\
						 str,			\
						 true);			\
		}							\
	}								\
} while (/* CONSTCOND */ 0)

	/*
	 * If driver provides a method to retrieve its internal limit
	 * values, call it and use those returned values as initial
	 * limits for event monitoring.
	 */
	props = 0;
	if (sed_t->sed_edata->flags & ENVSYS_FMONLIMITS)
		if (sed_t->sed_sme->sme_get_limits)
			(*sed_t->sed_sme->sme_get_limits)(sed_t->sed_sme,
							  sed_t->sed_edata,
							  &lims, &props);
	/*
	 * If driver doesn't provide a way to "absorb" user-specified
	 * limit values, we must monitor all limits ourselves
	 */
	if (sed_t->sed_sme->sme_set_limits == NULL)
		props &= ~PROP_DRIVER_LIMITS;

	/* Register the events that were specified */

	SEE_REGEVENT(ENVSYS_FMONCRITICAL,
		     PENVSYS_EVENT_CRITICAL,
		     "critical");

	SEE_REGEVENT(ENVSYS_FMONSTCHANGED,
		     PENVSYS_EVENT_STATE_CHANGED,
		     "state-changed");

	SEE_REGEVENT(ENVSYS_FMONLIMITS,
		     PENVSYS_EVENT_LIMITS,
		     "hw-range-limits");

	/* 
	 * we are done, free memory now.
	 */
	kmem_free(sed_t, sizeof(*sed_t));
}

/*
 * sme_events_init:
 *
 * 	+ Initialize the events framework for this device.
 */
int
sme_events_init(struct sysmon_envsys *sme)
{
	int error = 0;
	uint64_t timo;

	KASSERT(sme != NULL);
	KASSERT(mutex_owned(&sme->sme_mtx));

	if (sme->sme_events_timeout)
		timo = sme->sme_events_timeout * hz;
	else
		timo = SME_EVTIMO;

	error = workqueue_create(&sme->sme_wq, sme->sme_name,
	    sme_events_worker, sme, PRI_NONE, IPL_SOFTCLOCK, WQ_MPSAFE);
	if (error)
		return error;

	mutex_init(&sme->sme_callout_mtx, MUTEX_DEFAULT, IPL_SOFTCLOCK);
	callout_init(&sme->sme_callout, CALLOUT_MPSAFE);
	callout_setfunc(&sme->sme_callout, sme_events_check, sme);
	callout_schedule(&sme->sme_callout, timo);
	sme->sme_flags |= SME_CALLOUT_INITIALIZED;
	DPRINTF(("%s: events framework initialized for '%s'\n",
	    __func__, sme->sme_name));

	return error;
}

/*
 * sme_events_destroy:
 *
 * 	+ Destroys the event framework for this device: callout
 * 	  stopped, workqueue destroyed and callout mutex destroyed.
 */
void
sme_events_destroy(struct sysmon_envsys *sme)
{
	KASSERT(mutex_owned(&sme->sme_mtx));

	callout_stop(&sme->sme_callout);
	workqueue_destroy(sme->sme_wq);
	mutex_destroy(&sme->sme_callout_mtx);
	callout_destroy(&sme->sme_callout);
	sme->sme_flags &= ~SME_CALLOUT_INITIALIZED;
	DPRINTF(("%s: events framework destroyed for '%s'\n",
	    __func__, sme->sme_name));
}

/*
 * sysmon_envsys_update_limits
 *
 *	+ If a driver needs to update the limits that it is providing,
 *	  we need to update the dictionary data as well as the limits.
 *	  This only makes sense if the driver is capable of providing
 *	  its limits, and if there is a limits event-monitor.
 */
int
sysmon_envsys_update_limits(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	int err;

	sysmon_envsys_acquire(sme, false);
	if (sme->sme_get_limits == NULL ||
	    (edata->flags & ENVSYS_FMONLIMITS) == 0)
		err = EINVAL;
	else
		err = sme_update_limits(sme, edata);
	sysmon_envsys_release(sme, false);

	return err;
}

/*
 * sme_update_limits
 *
 *	+ Internal version of sysmon_envsys_update_limits() to be used
 *	  when the device has already been sysmon_envsys_acquire()d.
 */

int
sme_update_limits(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	prop_dictionary_t sdict = NULL;
	prop_array_t array = NULL;
	sysmon_envsys_lim_t lims;
	sme_event_t *see;
	uint32_t props = 0;

	/* Find the dictionary for this sensor */
	array = prop_dictionary_get(sme_propd, sme->sme_name);
	if (array == NULL ||
	    prop_object_type(array) != PROP_TYPE_ARRAY) {
		DPRINTF(("%s: array device failed\n", __func__));
		return EINVAL;
	}
	
	sdict = prop_array_get(array, edata->sensor);
	if (sdict == NULL) {
		return EINVAL;
	}

	/* Find the event definition to get its powertype */
	LIST_FOREACH(see, &sme->sme_events_list, see_list) {
		if (edata == see->see_edata &&
		    see->see_type == PENVSYS_EVENT_LIMITS)
			break;
	}
	if (see == NULL)
		return EINVAL;

	/* Update limit values from driver if possible */
	if (sme->sme_get_limits != NULL)
		(*sme->sme_get_limits)(sme, edata, &lims, &props);

	/* Update event and dictionary */
	sme_event_register(sdict, edata, sme, &lims, props,
			   PENVSYS_EVENT_LIMITS, see->see_pes.pes_type);

	return 0;
}

/*
 * sme_events_check:
 *
 * 	+ Passes the events to the workqueue thread and stops
 * 	  the callout if the 'low-power' condition is triggered.
 */
void
sme_events_check(void *arg)
{
	struct sysmon_envsys *sme = arg;
	sme_event_t *see;
	uint64_t timo;

	KASSERT(sme != NULL);

	mutex_enter(&sme->sme_callout_mtx);
	LIST_FOREACH(see, &sme->sme_events_list, see_list) {
		workqueue_enqueue(sme->sme_wq, &see->see_wk, NULL);
		see->see_edata->flags |= ENVSYS_FNEED_REFRESH;
	}
	if (sme->sme_events_timeout)
		timo = sme->sme_events_timeout * hz;
	else
		timo = SME_EVTIMO;
	if (!sysmon_low_power)
		callout_schedule(&sme->sme_callout, timo);
	mutex_exit(&sme->sme_callout_mtx);
}

/*
 * sme_events_worker:
 *
 * 	+ workqueue thread that checks if there's a critical condition
 * 	  and sends an event if it was triggered.
 */
void
sme_events_worker(struct work *wk, void *arg)
{
	sme_event_t *see = (void *)wk;
	struct sysmon_envsys *sme = see->see_sme;
	envsys_data_t *edata = see->see_edata;

	KASSERT(wk == &see->see_wk);
	KASSERT(sme != NULL || edata != NULL);

	mutex_enter(&sme->sme_mtx);
	see->see_flags |= SEE_EVENT_WORKING;
	/* 
	 * sme_events_check marks the sensors to make us refresh them here.
	 * Don't refresh if the driver uses its own method for refreshing.
	 */
	if ((sme->sme_flags & SME_DISABLE_REFRESH) == 0) {
		if ((edata->flags & ENVSYS_FNEED_REFRESH) != 0) {
			/* refresh sensor in device */
			(*sme->sme_refresh)(sme, edata);
			edata->flags &= ~ENVSYS_FNEED_REFRESH;
		}
	}

	DPRINTFOBJ(("%s: (%s) desc=%s sensor=%d type=%d state=%d units=%d "
	    "value_cur=%d upropset=%d\n", __func__, sme->sme_name, edata->desc,
	    edata->sensor, see->see_type, edata->state, edata->units,
	    edata->value_cur, edata->upropset));

	/* skip the event if current sensor is in invalid state */
	if (edata->state == ENVSYS_SINVALID)
		goto out;

	/*
	 * For range limits, if the driver claims responsibility for
	 * limit/range checking, just user driver-supplied status.
	 * Else calculate our own status.  Note that driver must
	 * relinquish responsibility for ALL limits if there is even
	 * one limit that it cannot handle!
	 *
	 * If this is a CAPACITY monitor, but the sensor's max_value
	 * is not set, treat it as though the monitor does not exist.
	 */
	if ((see->see_type == PENVSYS_EVENT_LIMITS ||
	     see->see_type == PENVSYS_EVENT_CAPACITY) &&
	    (edata->upropset & PROP_DRIVER_LIMITS) == 0) {
		if ((see->see_type == PENVSYS_EVENT_CAPACITY) &&
		    (edata->value_max == 0))
			edata->state = ENVSYS_SVALID;
		else if ((edata->upropset & (PROP_CRITMIN | PROP_BATTCAP)) &&
		    (edata->value_cur < edata->limits.sel_critmin))
			edata->state = ENVSYS_SCRITUNDER;
		else if ((edata->upropset & (PROP_WARNMIN | PROP_BATTWARN)) &&
			 (edata->value_cur < edata->limits.sel_warnmin))
			edata->state = ENVSYS_SWARNUNDER;
		else if ((edata->upropset & (PROP_CRITMAX | PROP_BATTMAX)) &&
			 (edata->value_cur > edata->limits.sel_critmax))
			edata->state = ENVSYS_SCRITOVER;
		else if ((edata->upropset & (PROP_WARNMAX | PROP_BATTHIGH)) &&
			 (edata->value_cur > edata->limits.sel_warnmax))
			edata->state = ENVSYS_SWARNOVER;
		else
			edata->state = ENVSYS_SVALID;
	}
	sme_deliver_event(see);

out:
	see->see_flags &= ~SEE_EVENT_WORKING;
	cv_broadcast(&sme->sme_condvar);
	mutex_exit(&sme->sme_mtx);
}

/*
 * sysmon_envsys_sensor_event
 *
 *	+ Find the monitor event of a particular type for a given sensor
 *	  on a device and deliver the event if one is required.  If
 *	  no event type is specified, deliver all events for the sensor.
 */
void
sysmon_envsys_sensor_event(struct sysmon_envsys *sme, envsys_data_t *edata, 
			   int ev_type)
{
	sme_event_t *see;

	mutex_enter(&sme->sme_mtx);
	LIST_FOREACH(see, &sme->sme_events_list, see_list) {
		if (edata != see->see_edata)
			continue;
		if (ev_type == 0 ||
		    ev_type == see->see_type) {
			sme_deliver_event(see);
			if (ev_type != 0)
				break;
		}
	}
	mutex_exit(&sme->sme_mtx);
}

/*
 * sme_deliver_event:
 *
 * 	+ If new sensor state requires it, send an event to powerd
 *
 *	  Must be called with the device's sysmon mutex held
 *		see->see_sme->sme_mtx
 */
void
sme_deliver_event(sme_event_t *see)
{
	envsys_data_t *edata = see->see_edata;
	const struct sme_description_table *sdt = NULL;
	const struct sme_sensor_event *sse = sme_sensor_event;
	int i, state = 0;

	switch (see->see_type) {
	case PENVSYS_EVENT_LIMITS:
	case PENVSYS_EVENT_CAPACITY:
		/*
		 * Send event if state has changed
		 */
		if (edata->state == see->see_evsent)
			break;

		for (i = 0; sse[i].state != -1; i++)
			if (sse[i].state == edata->state)
				break;

		if (sse[i].state == -1)
			break;

		if (edata->state == ENVSYS_SVALID)
			sysmon_penvsys_event(&see->see_pes,
					     PENVSYS_EVENT_NORMAL);
		else
			sysmon_penvsys_event(&see->see_pes, sse[i].event);

		see->see_evsent = edata->state;
		DPRINTFOBJ(("%s: (%s) desc=%s sensor=%d state=%d send_ev=%d\n",
		    __func__, see->see_sme->sme_name, edata->desc,
		    edata->sensor, edata->state,
		    (edata->state == ENVSYS_SVALID) ? PENVSYS_EVENT_NORMAL :
			sse[i].event));

		break;

	/*
	 * Send PENVSYS_EVENT_CRITICAL event if:
	 *	State has gone from non-CRITICAL to CRITICAL,
	 *	State remains CRITICAL and value has changed, or
	 *	State has returned from CRITICAL to non-CRITICAL
	 */
	case PENVSYS_EVENT_CRITICAL:
		if (edata->state == ENVSYS_SVALID &&
		    see->see_evsent != 0) {
			sysmon_penvsys_event(&see->see_pes,
					     PENVSYS_EVENT_NORMAL);
			see->see_evsent = 0;
		} else if (edata->state == ENVSYS_SCRITICAL &&
		    see->see_evsent != edata->value_cur) {
			sysmon_penvsys_event(&see->see_pes,
					     PENVSYS_EVENT_CRITICAL);
			see->see_evsent = edata->value_cur;
		}
		break;

	/*
	 * if value_cur is not normal (battery) or online (drive),
	 * send the event...
	 */
	case PENVSYS_EVENT_STATE_CHANGED:
		/* 
		 * the state has not been changed, just ignore the event.
		 */
		if (edata->value_cur == see->see_evsent)
			break;

		switch (edata->units) {
		case ENVSYS_DRIVE:
			sdt = sme_get_description_table(SME_DESC_DRIVE_STATES);
			state = ENVSYS_DRIVE_ONLINE;
			break;
		case ENVSYS_BATTERY_CAPACITY:
			sdt = sme_get_description_table(
			    SME_DESC_BATTERY_CAPACITY);
			state = ENVSYS_BATTERY_CAPACITY_NORMAL;
			break;
		default:
			panic("%s: bad units for PENVSYS_EVENT_STATE_CHANGED",
			    __func__);
		}

		for (i = 0; sdt[i].type != -1; i++)
			if (sdt[i].type == edata->value_cur)
				break;

		if (sdt[i].type == -1)
			break;

		/* 
		 * copy current state description.
		 */
		(void)strlcpy(see->see_pes.pes_statedesc, sdt[i].desc,
		    sizeof(see->see_pes.pes_statedesc));

		if (edata->value_cur == state)
			/*
			 * state returned to normal condition
			 */
			sysmon_penvsys_event(&see->see_pes,
					     PENVSYS_EVENT_NORMAL);
		else
			/*
			 * state changed to abnormal condition
			 */
			sysmon_penvsys_event(&see->see_pes, see->see_type);

		see->see_evsent = edata->value_cur;

		/* 
		 * There's no need to continue if it's a drive sensor.
		 */
		if (edata->units == ENVSYS_DRIVE)
			break;

		/*
		 * Check if the system is running in low power and send the
		 * event to powerd (if running) or shutdown the system
		 * otherwise.
		 */
		if (!sysmon_low_power && sme_event_check_low_power()) {
			struct penvsys_state pes;

			/*
			 * Stop the callout and send the 'low-power' event.
			 */
			sysmon_low_power = true;
			callout_stop(&see->see_sme->sme_callout);
			pes.pes_type = PENVSYS_TYPE_BATTERY;
			sysmon_penvsys_event(&pes, PENVSYS_EVENT_LOW_POWER);
		}
		break;
	default:
		panic("%s: invalid event type %d", __func__, see->see_type);
	}
}

/*
 * Returns true if the system is in low power state: an AC adapter
 * is OFF and all batteries are in LOW/CRITICAL state.
 */
static bool
sme_event_check_low_power(void)
{
	if (!sme_acadapter_check())
		return false;

	return sme_battery_check();
}

/*
 * Called with the sysmon_envsys device mtx held through the
 * workqueue thread.
 */
static bool
sme_acadapter_check(void)
{
	struct sysmon_envsys *sme;
	envsys_data_t *edata;
	bool dev = false, sensor = false;

	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {
		if (sme->sme_class == SME_CLASS_ACADAPTER) {
			dev = true;
			break;
		}
	}

	/*
	 * No AC Adapter devices were found.
	 */
	if (!dev)
		return false;

	/*
	 * Check if there's an AC adapter device connected.
	 */
	TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
		if (edata->units == ENVSYS_INDICATOR) {
			sensor = true;
			/* refresh current sensor */
			if ((sme->sme_flags & SME_DISABLE_REFRESH) == 0)
				(*sme->sme_refresh)(sme, edata);
			if (edata->value_cur)
				return false;
		}
	}

	if (!sensor)
		return false;

	/* 
	 * AC adapter found and not connected.
	 */
	return true;
}

/*
 * Called with the sysmon_envsys device mtx held through the
 * workqueue thread.
 */
static bool
sme_battery_check(void)
{
	struct sysmon_envsys *sme;
	envsys_data_t *edata;
	int batteriesfound = 0;
	bool present, batterycap, batterycharge;

	/*
	 * Check for battery devices and its state.
	 */
	LIST_FOREACH(sme, &sysmon_envsys_list, sme_list) {
		if (sme->sme_class != SME_CLASS_BATTERY)
			continue;

		present = true;
		TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
			if (edata->units == ENVSYS_INDICATOR &&
			    !edata->value_cur) {
				present = false;
				break;
			}
		}
		if (!present)
			continue;
		/*
		 * We've found a battery device...
		 */
		batteriesfound++;
		batterycap = batterycharge = false;
		TAILQ_FOREACH(edata, &sme->sme_sensors_list, sensors_head) {
			if (edata->units == ENVSYS_BATTERY_CAPACITY) {
				batterycap = true;
				if (!sme_battery_critical(edata))
					return false;
			} else if (edata->units == ENVSYS_BATTERY_CHARGE) {
				batterycharge = true;
				if (edata->value_cur)
					return false;
			}
		}
		if (!batterycap || !batterycharge)
			return false;
	}

	if (!batteriesfound)
		return false;

	/*
	 * All batteries in low/critical capacity and discharging.
	 */
	return true;
}

static bool
sme_battery_critical(envsys_data_t *edata)
{
	if (edata->value_cur == ENVSYS_BATTERY_CAPACITY_CRITICAL ||
	    edata->value_cur == ENVSYS_BATTERY_CAPACITY_LOW)
		return true;

	return false;
}
