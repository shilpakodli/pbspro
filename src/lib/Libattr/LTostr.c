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

#include <pbs_config.h>   /* the master config generated by configure */

#include "Long.h"
/**
 * @file	LTostr.c
 *
 * @brief
 * 	LTostr - returns a pointer to the character string representation of the
 *	Long number, value, represented in base, base.
 */

/**
 * @brief
 * 	LTostr - returns a pointer to the character string representation of the
 *	Long number, value, represented in base, base.
 *
 *	If base is outside its domain of 2 through the number of characters in
 *	the Long_dig array, uLTostr returns a zero-length string and sets errno
 *	to EDOM.
 *
 * 	The string is stored in a static array and will be clobbered the next
 *	time either LTostr or uLTostr is called.  The price of eliminating the
 * 	possibility of memory leaks is the necessity to copy the string
 *	immediately to a safe place if it must last.
 *
 * @param[in] value - Long value
 * @param[in] base - base representation of val
 *
 * @return	string
 * @retval	char reprsn of Long	Success
 * @retval	NULL			error
 *
 */

const char *
LTostr(Long value, int base)
{
	if (value < 0) {
		char	*bp;

		bp = (char *)uLTostr((u_Long)-value, base);
		if (*bp == '\0')
			return (bp);
		*--bp = '-';
		return (bp);
	}
	return (uLTostr((u_Long)value, base));
}
