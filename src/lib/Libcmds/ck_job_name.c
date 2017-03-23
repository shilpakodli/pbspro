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

#include "pbs_ifl.h"
#include <pbs_config.h>   /* the master config generated by configure */

#include "cmds.h"
/**
 * @file	ck_job_name.c
 */

/**
 * @brief
 * 	isalnumspch = if char is alpha numeric or from allowed set of special char
 *
 * @param[in] c - input
 *
 * @return	int
 */
static int
isalnumspch(int c)
{
	if (isalnum(c) != 0)
		return c;

	if (c == '-' || c == '_' || c == '+')
		return c;

	return 0;
}

/**
 * @brief
 *	validates the job name
 * 	check_job_name = job name must be <= PBS_MAXJOBNAME "printable" characters with
 *	first alphabetic, maybe.  The POSIX Batch standard calls for only
 *	alphanumeric, but then conflicts with itself to default to the
 *	script base-name which may have non-alphanumeric characters and
 *	the first character not alphabetic.
 *
 *	We check for visible, printable characters and the first being
 *	alphabetic if comining from a -N option (chk_alpha = 1).
 *
 * @param[in]       name        - job name
 * @param[in]	    chk_alpha   - flag to allow numeric first char
 *
 * @return int
 * @retval 0    validation of job name was successful.
 * @retval -1   illeagal character in job name.	
 * @retval -2	job name length is too long.
 */
int
check_job_name(name, chk_alpha)
char *name;
int   chk_alpha;
{

	if (strlen(name) > (size_t)PBS_MAXJOBNAME)
		return (-2);
	else if ((chk_alpha == 1) && (isalpha((int)*name) == 0))
		return (-1);

	/* if caller is job submission request then allow 1st char to be
	 * only alpha or numeric or permitted special char
	 */
	if ((chk_alpha == 0) && (isalnumspch((int)*name) == 0)) {
		return (-1);
	}

	while (*name) {
		if (isgraph((int)*name++) == 0)	/* disallow any non-printing */
			return (-1);
	}
	return (0);
}
