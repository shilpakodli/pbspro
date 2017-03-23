/*
 * Copyright (C) 1994-2017 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *  
 * This file is part of the PBS Professional ("PBS Pro") software.
 * 
 * Open Source License Information:
 *  
 * PBS Pro is free software. You can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) any 
 * later version.
 *  
 * PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.
 *  
 * You should have received a copy of the GNU Affero General Public License along 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 * Commercial License Information: 
 * 
 * The PBS Pro software is licensed under the terms of the GNU Affero General 
 * Public License agreement ("AGPL"), except where a separate commercial license 
 * agreement for PBS Pro version 14 or later has been executed in writing with Altair.
 *  
 * Altair’s dual-license business model allows companies, individuals, and 
 * organizations to create proprietary derivative works of PBS Pro and distribute 
 * them - whether embedded or bundled with other software - under a commercial 
 * license agreement.
 * 
 * Use of Altair’s trademarks, including but not limited to "PBS™", 
 * "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's 
 * trademark licensing policies.
 *
 */
/**
 * @file	pbs_alterjob.c
 * @brief
 * Send the Alter Job request to the server --
 * really an instance of the "manager" request.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <stdlib.h>
#include "libpbs.h"


/**
 * @brief
 *	-Send the Alter Job request to the server --
 *	really an instance of the "manager" request.
 *
 * @param[in] c - connection handle
 * @param[in] jobid- job identifier
 * @param[in] attrib - pointer to attribute list
 * @param[in] extend - extend string for encoding req
 *
 * @return	int
 * @retval	0	success
 * @retval	!0	error
 *
 */
int
__pbs_alterjob(int c, char *jobid, struct attrl *attrib, char *extend)
{
	struct attropl *ap = (struct attropl *)NULL;
	struct attropl *ap1 = (struct attropl *)NULL;
	int i;

	if ((jobid == (char *)0) || (*jobid == '\0'))
		return (pbs_errno = PBSE_IVALREQ);

	/* copy the attrl to an attropl */
	while (attrib != (struct attrl *)NULL) {
		if (ap == (struct attropl *)NULL) {
			ap1 = ap = MH(struct attropl);
		} else {
			ap->next = MH(struct attropl);
			ap = ap->next;
		}
		if (ap == (struct attropl *)NULL) {
			pbs_errno = PBSE_SYSTEM;
			return -1;
		}
		ap->name = attrib->name;
		ap->resource = attrib->resource;
		ap->value = attrib->value;
		ap->op = SET;
		ap->next = (struct attropl *)NULL;
		attrib = attrib->next;
	}

	i = PBSD_manager(c,
		PBS_BATCH_ModifyJob,
		MGR_CMD_SET,
		MGR_OBJ_JOB,
		jobid,
		ap1,
		extend);

	/* free up the attropl we just created */
	while (ap1 != (struct attropl *)NULL) {
		ap = ap1->next;
		free(ap1);
		ap1 = ap;
	}

	return i;
}
