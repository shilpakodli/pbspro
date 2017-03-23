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
 * @file	site_mom_chu.c
 * @brief
 * site_mom_chu.c = a site modifible file
 */

#include <pbs_config.h>   /* the master config generated by configure */

/*
 * This is used only in mom and needs PBS_MOM defined in order to
 * have things from other .h files (such as struct task) be defined
 */
#define PBS_MOM

#include <sys/types.h>
#include <pwd.h>
#include "portability.h"
#include "list_link.h"
#include "server_limits.h"
#include "attribute.h"
#include "job.h"
#include "mom_mach.h"
#include "mom_func.h"


/**
 * @brief
 * 	site_mom_chkuser() - for adding validity checking to the user's account
 * 	on the execution machine.  This is called from start_exec().
 *
 * @param[in] pjob - job pointer
 * 
 * @return	int
 * @retval	non-zero if you wish the job to be aborted.
 * 
 */
int
site_mom_chkuser(job *pjob)
{
	return (0);
}

