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
 * @file    checkkey.c
 *
 * @brief
 *		attr_recov.c - The 'checkkey.c' file goes with the source package is used to verify licesnse
 *
 * Included public functions are:
 *
 *	pbs_get_hostid	returning host id.
 *	init_license	initialize values of license structure
 */

#include <pbs_config.h>   /* the master config generated by configure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "portability.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_ifl.h"
#include "pbs_nodes.h"
#include "server.h"
#include "svrfunc.h"
#include "net_connect.h"
#include "pbs_error.h"
#include "log.h"
#include "pbs_license.h"
#include "work_task.h"
#include "job.h"



char *pbs_licensing_license_location  = NULL;
long pbs_min_licenses		= PBS_MIN_LICENSING_LICENSES;
long pbs_max_licenses		= PBS_MAX_LICENSING_LICENSES;
int  pbs_licensing_linger	= PBS_LIC_LINGER_TIME;

/* Global Data Items: */
extern pbs_list_head svr_alljobs;
extern pbs_list_head svr_unlicensedjobs;

extern pbs_net_t pbs_server_addr;
unsigned long hostidnum;
int ext_license_server = 0;
int license_expired = 0;

void
check_expired_lic(struct work_task *ptask)
{
}

void
return_licenses(struct work_task *ptask)
{
}

int
pbs_get_licenses(int nlicense)
{
	return (0);
}

void
init_licensing(void)
{
}

int
status_licensing(void)
{
	return (0);
}

int
checkin_licensing(void)
{
	return (0);
}

void
close_licensing(void)
{
}
/**
 * @brief
 * 		returning host id.
 *
 * @return      Host ID
 */
unsigned long
pbs_get_hostid(void)
{
	unsigned long hid;

	hid = (unsigned long)gethostid();
	if (hid == 0)
		hid = (unsigned long)pbs_server_addr;
	return hid;
}
/**
 * @brief
 * 		initialize values of license structure
 *
 * @param[in]	licenses - pointer to the license block structure
 */
void
init_license(struct license_block *licenses)
{
	licenses->lb_trial = 0;
	licenses->lb_glob_floating = 10000000;
	licenses->lb_aval_floating = 10000000;
	licenses->lb_used_floating = 0;
	licenses->lb_high_used_floating = 0;
	licenses->lb_do_task = 0;
}

int
check_license(struct license_block *licenses)
{
	hostidnum = pbs_get_hostid();
	return (0);
}



/*
 ************************************************************************
 *
 * 		Licensing Jobs Functions
 *
 ************************************************************************
 */

int
set_cpu_licenses_need(job *pjob, char *exec_vnode)
{
	return 1;
}

static void
report_license_highuse(void)
{
}

void
allocate_cpu_licenses(job *pjob)
{
	if (pjob == NULL) {
		log_err(PBSE_INTERNAL, "allocate_cpu_licenses",
			"pjob is NULL so no action taken");
		return;
	}
	/* The following line works around the check in set_nodes() */
	pjob->ji_licalloc = 1;
}

void
deallocate_cpu_licenses(job *pjob)
{
}

void
clear_and_populate_svr_unlicensedjobs(void)
{
}

void
relicense_svr_unlicensedjobs(void)
{

}
