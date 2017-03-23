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
 * @file	pbs_stathost.c
 * @brief
 * Return the combined status of the vnodes on a host or all hosts.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include "libpbs.h"
#include "attribute.h"


/* data structure and local data items private to this file */

/* This structure is used to determined the set of separate "hosts"   */
struct host_list {
	char hl_name[PBS_MAXHOSTNAME+1];  /* host value */
	struct batch_status *hl_node;     /* count of vnodes in host */
};

/* This next structure is used to track and sum consumable resources */

struct consumable {
	char      *cons_resource;
	char      *cons_avail_str;	/* value in r_available if not consummable  */
	long long  cons_avail_sum;	/* sum of values in r_available if consumm. */
	long long  cons_assn_sum;	/* sum of values in r_assigned  if consumm. */
	short      cons_k;		/* set if "size" type (kb) */
	short      cons_consum;	/* set if resource is consumable */
	short      cons_set;	/* set if resource has a value */
};

/*
 * get_resource_value - for the named resource in the indicated attribute,
 *	resources_assigned or resources_available, return the value of the
 *	resource (as a string).   NULL is returned if the resources isn't there
 */

static char *
get_resource_value(char *attrn, char *rname, struct attrl *pal)
{
	while (pal) {
		if ((strcasecmp(pal->name, attrn) == 0) &&
			(strcasecmp(pal->resource, rname) == 0))
			return pal->value;
		pal = pal->next;
	}
	return NULL;
}

/**
 * @brief
 * add_consumable_entry - add an entry for a resource into the consumable array
 *
 * @par If the resource is found in resources_assigned, it is considered
 *	to be "consummable" and the various values are added together;
 *	- the resource is "consumable" and it is so flagged.
 *	If the resource is already in the table the consumable flag is updated
 *
 * @par If the resource is not consumable, the value string from the attrib
 *	is used acording to the following rules:
 *	 - If cons_avail_str is null, use the attrib value
 *         else if cons_avail_str == attrib value, no change
 *         else repace cons_avail_str with "<various>"
 *
 * @param[in]	patl	Pointer to element in attribute list
 * @param[in]	consum_flag	Whether the resource is consumable
 * @param[in]	consum	The array of consumables
 * @param[in]	consum	Number of elements in consum array
 *
 * @return	void
 */
static void
add_consumable_entry(struct attrl *patl,
	int consum_flag,
	struct consumable **consum,
	int *consumable_size)
{
	int i;
	struct consumable *newc;

	if (!patl || !consum || !consumable_size)
		return;

	/* Ignore indirect resources (those that contain '@') */
	if (strchr(patl->value, '@') != NULL)
		return;

	for (i=0; i<*consumable_size; ++i) {
		if (((*consum)+i) == NULL)
			continue;
		if (strcasecmp(patl->resource, ((*consum)+i)->cons_resource) == 0) {
			((*consum)+i)->cons_consum |= consum_flag;
			break;
		}
	}
	if (i == *consumable_size) {
		/* need to add this resource */
		newc = realloc(*consum, sizeof(struct consumable) * (*consumable_size+1));
		if (newc) {
			*consum = newc;
			(*consumable_size)++;
			if ((((*consum)+i)->cons_resource = strdup(patl->resource)) == NULL) {
				free(newc);
				(*consumable_size)--;
				pbs_errno = PBSE_SYSTEM;
				return;
			}
			((*consum)+i)->cons_avail_str = NULL;
			((*consum)+i)->cons_avail_sum = 0;
			((*consum)+i)->cons_assn_sum  = 0;
			((*consum)+i)->cons_k         = 0;
			((*consum)+i)->cons_consum    = consum_flag;
			((*consum)+i)->cons_set       = 0;
		} else {
			pbs_errno = PBSE_SYSTEM;
			return;		/* realloc failed, just return */
		}
	}
	/* Set cons_k for size values */
	if (strpbrk(patl->value, "kKmMgGtTpPbBwW") != NULL)
		((*consum)+i)->cons_k = 1;
}



/**
 * @brief
 * 	build_host_list - performs two functions while running through the vnodes
 *	1. builds a list of the various host names found in
 *	   resource_available.host
 *	2. determines which resources are in resources_assigned to know
 *	   which are consumable (and should be summed together).
 */

static void
build_host_list(struct batch_status *pbst,
	struct host_list  **phost_list,
	int *host_list_size,
	struct consumable **consum,
	int *consumable_size)
{
	int                i;
	char              *hostn;
	struct host_list  *newhl;
	struct attrl      *patl;

	/* clear existing entries for reuse */

	for (i=0; i<*host_list_size; ++i) {
		((*phost_list)+i)->hl_name[0] = '\0';
		((*phost_list)+i)->hl_node    = NULL;
	}

	while (pbst) {

		/* if need be, add host_list entry for this host */
		hostn = get_resource_value(ATTR_rescavail, "host", pbst->attribs);
		if (hostn) {
			for (i=0; i<*host_list_size; ++i) {
				if (strcasecmp(hostn, ((*phost_list)+i)->hl_name) == 0)
					break;
			}
			if (i == *host_list_size) {
				/* need to add slot for this host */
				newhl = (struct host_list *)realloc((*phost_list),
					sizeof(struct host_list) * (*host_list_size+1));
				if (newhl) {
					*phost_list = newhl;
					(*host_list_size)++;
				} else {
					pbs_errno = PBSE_SYSTEM;
					return;	/* memory allocation failure */
				}
				strcpy(((*phost_list)+i)->hl_name, hostn);
				((*phost_list)+i)->hl_node = pbst;

			} else {
				((*phost_list)+i)->hl_node  = NULL; /* null for multiple */
			}
		}

		/* now look to see what resources are in "resources_assigned" */

		patl = pbst->attribs;
		while (patl) {
			if (strcmp(patl->name, ATTR_rescavail) == 0)
				add_consumable_entry(patl, 0, consum, consumable_size);
			else if (strcmp(patl->name, ATTR_rescassn) == 0)
				add_consumable_entry(patl, 1, consum, consumable_size);
			patl = patl->next;
		}

		pbst = pbst->next;
	}

	return;
}

/**
 * @brief
 * sum_a_resources - add the value of the specified consumable resource
 *	into the "consumable" structure entry for that resource.
 *	"sized" valued resources are adjusted to be in "kb".
 *
 * @param[in]	psum	Pointer to resource
 * @param[in]	avail	Available or assigned list flag
 * @param[in]	value	Value of resource as a string
 * @param[in]	various	Label to use for multiple values
 *
 * @return	void
 */

static void
sum_a_resource(struct consumable *psum, int avail, char *value,
	char * various)
{
	long long  amt;
	char     *pc;

	if (!psum || !value || !various)
		return;

	if (psum->cons_consum == 0) {
		/* not a consumable resource */
		if (avail == 0)
			return;		/* this shouldn't happen, but no sweat */
		if (psum->cons_avail_str == NULL) {
			if ((psum->cons_avail_str = strdup(value)) == NULL) {
				pbs_errno = PBSE_SYSTEM;
				return;
			}
		} else if (strcasecmp(psum->cons_avail_str, value) != 0) {
			if (psum->cons_avail_str)
				free(psum->cons_avail_str);
			if ((psum->cons_avail_str = strdup(various)) == NULL) {
				pbs_errno = PBSE_SYSTEM;
				return;
			}
		}
		psum->cons_set = 1;
		return;
	}

	/* Ignore indirect resources (those that contain '@') */
	if (strchr(value, '@') != NULL)
		return;

	/* the resource is consumable so we need to add it to the current val */

	amt = strtol(value, &pc, 10);

	if (strpbrk(pc, "kKmMgGtTpPbBwW") != NULL) {

		/* adjust value to Kilobytes */
		if ((*pc == 'm')||(*pc == 'M'))
			amt = amt << 10;
		else if ((*pc == 'g') || (*pc == 'G'))
			amt = amt << 20;
		else if ((*pc == 't') || (*pc == 'T'))
			amt = amt << 30;
		else if ((*pc == 'p') || (*pc == 'P'))
			amt = amt << 30;
		else if ((*pc != 'k') && (*pc != 'K'))
			amt = amt >> 10;

		/* does the current sum need to be adjusted */
		if (psum->cons_k == 0) {
			psum->cons_avail_sum = psum->cons_avail_sum << 10;
			psum->cons_assn_sum =  psum->cons_assn_sum  << 10;
			psum->cons_k = 1;
		}
	}

	if (avail)
		psum->cons_avail_sum += amt;
	else
		psum->cons_assn_sum  += amt;

	psum->cons_set = 1;
}

/**
 * @brief
 * 	sum_resources - for each resource found in the collection of vnodes with
 *	the have "host", the resoruces in resources_available and
 *	resources_assigned are summed.
 */
static void
sum_resources(struct batch_status *pbs,
	struct batch_status *working,
	char *hostn,
	struct consumable *consum,
	int	consumable_size,
	char * various)
{
	char *curhn;
	int   i;
	char *val;

	/* clear sums */
	for (i=0; i<consumable_size; ++i) {
		if ((consum+i)->cons_avail_str) {
			free((consum+i)->cons_avail_str);
			(consum+i)->cons_avail_str = NULL;
		}
		(consum+i)->cons_avail_sum = 0;
		(consum+i)->cons_assn_sum  = 0;
	}

	while (pbs) {

		curhn = get_resource_value(ATTR_rescavail, "host", pbs->attribs);
		if (curhn && strcasecmp(curhn, hostn) == 0) {
			for (i=0; i<consumable_size; ++i) {

				val = get_resource_value(ATTR_rescavail, (consum+i)->cons_resource, pbs->attribs);
				sum_a_resource(consum+i, 1, val, various);

				val = get_resource_value(ATTR_rescassn, (consum+i)->cons_resource, pbs->attribs);
				sum_a_resource(consum+i, 0, val, various);
			}
		}

		pbs = pbs->next;
	}
}

/*
 * attr_names array - this array contains the possible names of vnode/host
 *	attributes to create for a "host" batch_status reply
 *	The values to be returned in the attrl list for the host are constructed
 *	in the an_value element and then converted into the attrls by the
 *	build_collective() function.
 */

#ifdef NAS /* localmod 012 */
#define	SKIP_FIRST	1
#define	SKIP_REST	2
#define	CATENATE	4
#define	UNIQUE		8
#define	SKIP_ALL	(SKIP_FIRST|SKIP_REST)

static struct attr_names {
	char *an_name;
	int   an_type;
	int   an_rest;
	char *an_value;
} attr_names[] = {
	{ ATTR_NODE_Mom,	UNIQUE, 	0,	NULL},
	{ ATTR_NODE_Port,	0, 	0,	NULL},
	{ ATTR_version,		0, 	0,	NULL},
	{ ATTR_NODE_ntype,	0, 	0,	NULL},
	{ ATTR_NODE_state,	UNIQUE, 	0,	NULL},
	{ ATTR_NODE_pcpus,	SKIP_REST, 	0,	NULL},
	{ ATTR_p,		0, 	0,	NULL},
	{ ATTR_NODE_jobs,	CATENATE|SKIP_FIRST, 	0,	NULL},
	{ ATTR_maxrun,		0, 	0,	NULL},
	{ ATTR_maxuserrun,	0, 	0,	NULL},
	{ ATTR_maxgrprun,	0, 	0,	NULL},
	{ ATTR_NODE_No_Tasks,	SKIP_REST, 	0,	NULL},
	{ ATTR_PNames,		0, 	0,	NULL},
	{ ATTR_NODE_resvs,	UNIQUE, 	0,	NULL},
	{ ATTR_queue,		UNIQUE, 	0,	NULL},
	{ ATTR_comment,		UNIQUE, 	0,	NULL},
	{ ATTR_NODE_resv_enable,	0, 	0,	NULL},
	{ ATTR_NODE_NoMultiNode,	0, 	0,	NULL},
	{ ATTR_NODE_Sharing,	UNIQUE, 	0,	NULL},
	{ ATTR_NODE_ProvisionEnable,	0,	0,	NULL},
	{ ATTR_NODE_current_aoe,	0,	0,	NULL},
	{ ATTR_NODE_License,	0,	0,	NULL},
	{ ATTR_NODE_LicenseInfo,	0,	0,	NULL},
	{ ATTR_NODE_TopologyInfo,	0,	0,	NULL},
	{ ATTR_rescavail,	SKIP_ALL,	0,	NULL},
	{ ATTR_rescassn,	SKIP_ALL,	0,	NULL},
	{(char *)0, 0, 0, NULL }
};

/**
 * @brief
 * 	build_collective - for each vnode in the original batch_status, pbs,
 *	apply the following rules to build "host" attributes in newbsr entry
 *	"the array" refers to the array attr_names[].an_value --
 *	1. if "resources_assigned" or "resources_available", skip for now
 *	2. else if that attribute in the array has a null value, dup the value
 *		2.5. but record the null if UNIQUE form of CATENATE
 *	3. else if CATENATE attribute, append string to that in the array
 *		3.5. possibly suppress duplicates
 *	4. else if pbs attribute and array value are different, set the array
 *		to "<various>" if not already so set
 *	5. Then add in the resources_available/assigned from the consum struct
 */

static void
build_collective(struct batch_status *pbs,
	struct batch_status *newbsr,
	char *hostn,
	struct consumable *consum,
	int	consumable_size,
	char *various)
{
	char		     convtbuf[256];
	struct attrl	    *cupatl;
	struct attrl	    *hdpatl;
	struct attrl	    *nwpatl;
	char 		    *curhn;
	char		    *dup;
	char		    *sp;
	int		     i;
	size_t		     len;

	for (i = 0; attr_names[i].an_name != NULL; ++i) {
		attr_names[i].an_rest = 0;
		attr_names[i].an_value = NULL;
	}

	for (; pbs; pbs =  pbs->next) {
		if (pbs->attribs == NULL)
			continue;	/* move out because it was a single host */
		curhn = get_resource_value(ATTR_rescavail, "host", pbs->attribs);
		if (curhn == NULL)
			continue;
		if (strcasecmp(hostn, curhn) != 0)
			continue;
		for (cupatl = pbs->attribs; cupatl; cupatl = cupatl->next) {
			for (i = 0; attr_names[i].an_name != NULL; ++i) {
				int rest, type;
				if (strcmp(attr_names[i].an_name, cupatl->name) != 0)
					continue;
				type = attr_names[i].an_type;
				rest = attr_names[i].an_rest;
				attr_names[i].an_rest = 1;
				if (rest == 0 && (type & SKIP_FIRST))
					break;
				if (rest && (type & SKIP_REST))
					break;
				if (!(type & UNIQUE) && attr_names[i].an_value == NULL) {
					/* rule 2. - replace null with this string */
					attr_names[i].an_value = strdup(cupatl->value);
					break;
				}
				if (type & UNIQUE) {
					if (attr_names[i].an_value == NULL) {
						if (rest == 0 || (type & SKIP_FIRST)) {
							attr_names[i].an_value = strdup(cupatl->value);
							break;
						}
						attr_names[i].an_value = strdup("<null>");
					}
					/* rule 3.5 suppress duplicates */
					len = strlen(cupatl->value);
					for (sp = attr_names[i].an_value; sp && *sp; ++sp) {
						sp = strstr(sp, cupatl->value);
						if (!sp)
							break;
						if (sp != attr_names[i].an_value
							&& sp[-1] != ' ') {
							continue;
						}
						if (sp[len] == ',' || sp[len] == '\0')
							break;
					}
					if (sp && *sp)
						break;
					/*
					 * Fall through to CATENATE
					 */
					type |= CATENATE;
				}
				if (type & CATENATE) {
					/* rule 3. - concatenate values */
					len = strlen(attr_names[i].an_value) +
						strlen(cupatl->value) + 3;
					/* 3 for comma, space, and null char */
					dup = malloc(len);
					if (dup) {
						strcpy(dup, attr_names[i].an_value);
						strcat(dup, ", ");
						strcat(dup, cupatl->value);
						free(attr_names[i].an_value);
						attr_names[i].an_value = dup;
					}
					break;
				}
				if ((strcmp(attr_names[i].an_value, various) != 0) && (strcmp(attr_names[i].an_value, cupatl->value) != 0)) {
					/* rule 4. - differing values = "<various>" */
					free(attr_names[i].an_value);
					attr_names[i].an_value = strdup(various);
				}
				break;
			}
		}
	}
#else
static struct attr_names {
	char *an_name;
	char *an_value;
} attr_names[] = {
	{ ATTR_NODE_Mom, NULL},
	{ ATTR_NODE_Port, NULL},
	{ ATTR_version, NULL},
	{ ATTR_NODE_ntype, NULL},
	{ ATTR_NODE_state, NULL},
	{ ATTR_NODE_pcpus, NULL},
	{ ATTR_p, NULL},
	{ ATTR_NODE_jobs, NULL},
	{ ATTR_maxrun, NULL},
	{ ATTR_maxuserrun, NULL},
	{ ATTR_maxgrprun, NULL},
	{ ATTR_NODE_No_Tasks, NULL},
	{ ATTR_PNames, NULL},
	{ ATTR_NODE_resvs, NULL},
	{ ATTR_queue, NULL},
	{ ATTR_comment, NULL},
	{ ATTR_NODE_resv_enable, NULL},
	{ ATTR_NODE_NoMultiNode, NULL},
	{ ATTR_NODE_Sharing, NULL},
	{ ATTR_NODE_ProvisionEnable, NULL},
	{ ATTR_NODE_current_aoe, NULL},
	{ ATTR_NODE_License, NULL},
	{ ATTR_NODE_LicenseInfo, NULL},
	{ ATTR_NODE_TopologyInfo, NULL},
	{ ATTR_NODE_VnodePool, NULL},
	{(char *)0, NULL }
};

/**
 * @brief
 * 	build_collective - for each vnode in the original batch_status, pbs,
 *	apply the following rules to build "host" attributes in newbsr entry
 *	"the array" refers to the array attr_names[].an_value --
 *	1. if "resources_assigned" or "resources_available", skip for now
 *	2. else if that attribute in the array has a null value, dup the value
 *	3. else if "jobs" attribute, append string to that in the array
 *	4. else if pbs attribute and array value are different, set the array
 *		to "<various>" if not already so set
 *	5. Then add in the resources_available/assigned from the consum struct
 */

static void
build_collective(struct batch_status *pbs,
	struct batch_status *newbsr,
	char *hostn,
	struct consumable *consum,
	int	consumable_size,
	char *various)
{
	char		     convtbuf[256];
	struct attrl	    *cupatl;
	struct attrl	    *hdpatl;
	struct attrl	    *nwpatl;
	char		    *curhn;
	char		    *dup;
	int		     i;
	size_t		     len;

	for (i = 0; attr_names[i].an_name != NULL; ++i)
		attr_names[i].an_value = NULL;

	for (; pbs; pbs =  pbs->next) {
		if (pbs->attribs == NULL)
			continue;	/* move out because it was a single host */
		curhn = get_resource_value(ATTR_rescavail, "host", pbs->attribs);
		if (curhn == NULL)
			continue;
		if (strcasecmp(hostn, curhn) == 0) {
			for (cupatl = pbs->attribs; cupatl; cupatl = cupatl->next) {
				if ((strcmp(cupatl->name, ATTR_rescavail) == 0) ||
					(strcmp(cupatl->name, ATTR_rescassn) == 0))
					continue;	/* rule 1. */

				for (i = 0; attr_names[i].an_name != NULL; ++i) {
					if (strcmp(attr_names[i].an_name, cupatl->name) == 0) {

						if (attr_names[i].an_value == NULL) {
							/* rule 2. - replace null with this string */
							if ((attr_names[i].an_value = strdup(cupatl->value)) == NULL) {
								pbs_errno = PBSE_SYSTEM;
								return;
							}
						} else if (strcmp(cupatl->name, ATTR_NODE_jobs)==0) {
							/* rule 3. - concatenate jobs */
							len = strlen(attr_names[i].an_value) +
								strlen(cupatl->value) + 3;
							/* 3 for comma, space, and null char */
							dup = malloc(len);
							if (dup) {
								strcpy(dup, attr_names[i].an_value);
								strcat(dup, ", ");
								strcat(dup, cupatl->value);
								free(attr_names[i].an_value);
							} else {
								pbs_errno = PBSE_SYSTEM;
								return;
							}
							attr_names[i].an_value = dup;
						} else if ((strcmp(attr_names[i].an_value, various) != 0) && (strcmp(attr_names[i].an_value, cupatl->value) != 0)) {
							/* rule 4. - differing values = "<various>" */
							free(attr_names[i].an_value);
							if ((attr_names[i].an_value = strdup(various)) == NULL) {
								pbs_errno = PBSE_SYSTEM;
								return;
							}
						}
					}
				}

			}
		}
	}
#endif /* localmod 012 */

	/* Turn the values saved in attr_names into attrl entries */
	/* any attr_names[].an_value with a null value is ignored */

	hdpatl = NULL;

	for (i=0; attr_names[i].an_name; ++i) {
		if (attr_names[i].an_value) {
			nwpatl = new_attrl();
			if (nwpatl) {
				if (hdpatl == NULL)
					hdpatl = nwpatl;	/* head of list */
				else
					cupatl->next = nwpatl;
				if ((nwpatl->name = strdup(attr_names[i].an_name)) == NULL) {
					free(nwpatl);
					pbs_errno = PBSE_SYSTEM;
					return;
				}
				nwpatl->value    = attr_names[i].an_value;
				/* note, the above is not dupped */
				attr_names[i].an_value = NULL;
				cupatl = nwpatl;
			} else {
				pbs_errno = PBSE_SYSTEM;
				return;
			}
		}
	}

	/* then  apply rule 5 - add in resources_available/assigned */
	/* takes two passes, first for resources_available and      */
	/* then for resources_assigned				    */

	for (i=0; i<consumable_size; ++i) {
		if ((consum+i)->cons_set == 0)
			continue;
		if ((consum+i)->cons_consum) {
			sprintf(convtbuf, "%lld", (consum+i)->cons_avail_sum);
			if ((consum+i)->cons_k)
				strcat(convtbuf, "kb");
			dup = convtbuf;
		} else {
			dup = (consum+i)->cons_avail_str;
		}
		if (dup != NULL) {
			nwpatl = malloc(sizeof(struct attrl));

			if (nwpatl) {
				if (hdpatl == NULL)
					hdpatl = nwpatl;
				else
					cupatl->next = nwpatl;
				nwpatl->next     = NULL;
				if ((nwpatl->name = strdup(ATTR_rescavail)) == NULL) {
					free(nwpatl);
					pbs_errno = PBSE_SYSTEM;
					return;
				}
				if ((nwpatl->resource = strdup((consum+i)->cons_resource)) == NULL) {
					free(nwpatl->name);
					free(nwpatl);
					pbs_errno = PBSE_SYSTEM;
					return;
				}
				if ((nwpatl->value = strdup(dup)) == NULL) {
					free(nwpatl->name);
					free(nwpatl->resource);
					free(nwpatl);
					pbs_errno = PBSE_SYSTEM;
					return;
				}
				cupatl = nwpatl;
			} else {
				pbs_errno = PBSE_SYSTEM;
				return;
			}
		}
	}

	/* now do the resources_assigned */
	for (i=0; i<consumable_size; ++i) {
		if ((consum+i)->cons_set == 0)
			continue;
		if ((consum+i)->cons_consum) {
			sprintf(convtbuf, "%lld", (consum+i)->cons_assn_sum);
			if ((consum+i)->cons_k)
				strcat(convtbuf, "kb");
			nwpatl = new_attrl();
			if (nwpatl) {
				if (hdpatl == NULL)
					hdpatl = nwpatl;
				else
					cupatl->next = nwpatl;
				if ((nwpatl->name = strdup(ATTR_rescassn)) == NULL) {
					free(nwpatl);
					pbs_errno = PBSE_SYSTEM;
					return;
				}
				if ((nwpatl->resource = strdup((consum+i)->cons_resource)) == NULL) {
					free(nwpatl->name);
					free(nwpatl);
					pbs_errno = PBSE_SYSTEM;
					return;
				}
				if ((nwpatl->value = strdup(convtbuf)) == NULL) {
					free(nwpatl->resource);
					free(nwpatl->name);
					free(nwpatl);
					pbs_errno = PBSE_SYSTEM;
					return;
				}
				cupatl = nwpatl;
			} else {
				pbs_errno = PBSE_SYSTEM;
				return;
			}
		}
	}

	/* NOTE: DO NOT free the attr_names[].an_value strings   */
	/* They are in use in the attrls sent back to the caller */

	newbsr->attribs = hdpatl;
}


/**
 * @brief
 * 	build_return_status_list - build a new batch_status list for the reply
 *	or append to the existing list which is passed in.
 */
struct batch_status *build_return_status(struct batch_status *bstatus,
	char *hname,
	struct batch_status *curlist,
	struct host_list  *phost_list,
	int    host_list_size,
	struct consumable *consum,
	int	consumable_size,
	char *various)
{
	struct batch_status *cpbs;
	struct batch_status *npbs;
	int                  i;

	npbs = malloc(sizeof(struct batch_status));
	if (npbs == NULL) {
		pbs_errno = PBSE_SYSTEM;
		return NULL;
	}

	npbs->next = NULL;
	npbs->text = NULL;

	/* is the host in question a single or multi-vnode host */

	for (i=0; i<host_list_size; ++i) {
		if (strcasecmp(hname, (phost_list+i)->hl_name) == 0) {
			if ((phost_list+i)->hl_node != NULL) {

				/* single vnode host - use the real one */

				/* prevent double free: copy name, move attribs */
				if ((npbs->name = strdup((phost_list+i)->hl_node->name)) == NULL) {
					free(npbs);
					pbs_errno = PBSE_SYSTEM;
					return NULL;
				}
				npbs->attribs = (phost_list+i)->hl_node->attribs;
				(phost_list+i)->hl_node->attribs = NULL;
				if ((phost_list+i)->hl_node->text)
					if ((npbs->text = strdup((phost_list+i)->hl_node->text)) == NULL) {
						free(npbs->name);
						free(npbs);
						pbs_errno = PBSE_SYSTEM;
						return NULL;
					}

			} else {

				/* multi-vnoded host, build attrls from collection */
				/* of the attrls of all the vnode's on the host    */

				if ((npbs->name = strdup(hname)) == NULL) {
					free(npbs);
					pbs_errno = PBSE_SYSTEM;
					return NULL;
				}
				npbs->attribs = NULL;

				sum_resources(bstatus,  npbs, hname,
					consum, consumable_size, various);
				build_collective(bstatus, npbs, hname,
					consum, consumable_size, various);
			}

			/* append new to end of the current list */
			if (curlist == NULL) {
				curlist = npbs;
			} else {
				/* curlist must be unchanged here */
				cpbs = curlist;
				while (cpbs->next)
					cpbs = cpbs->next;
				cpbs->next = npbs;
			}
			break;
		}
	}
	if (i == host_list_size) {
		/* did not find a host of the given name in the table */
		free(npbs);	/* no leaking */
		pbs_errno = PBSE_UNKNODE;
	}

	return curlist;
}

/**
 * @brief
 * 	pbs_stathost - return status on a single named host or all hosts known
 *	A host is defined by the value of resources_available.host
 *
 *	Function does a pbs_statvnode() to collect information on all vnodes
 *	and then aggregates the attributes from the vndoes that share the same
 *	host value.
 *
 *	If resources in resources_assigned/resources_available are consumable,
 *	so defined by being in resources_assigned, then the values for the same
 *	resource on the collection of vnodes are summed.
 *
 *	Otherwise, if the attribute or resource values are identical across the
 *	set of vnodes, that value is reported.  Else, the string "<various>"
 *	is reported, meaing the vnodes have different values.
 *
 *	This function, like most in PBS, is NOT thread safe.
 */

struct batch_status *
__pbs_stathost(int con, char *hid, struct attrl *attrib, char *extend)
{
	struct batch_status *breturn;	/* the list returned to the caller */
	struct batch_status *bstatus;	/* used internally		   */
	int   i;
	/* variables used across many function, these are what make the function */
	char		 *various         = "<various>";
	struct host_list  *phost_list      = NULL;
	struct consumable *consum          = NULL;
	int		  host_list_size  = 0;
	int		  consumable_size = 0;
	struct pbs_client_thread_connect_context *context;

	breturn = NULL;

	/* get status of all vnodes */
	bstatus = pbs_statvnode(con, "", attrib, extend);

	if (bstatus == NULL)
		return NULL;

	build_host_list(bstatus, &phost_list, &host_list_size,
		&consum, &consumable_size);

	if ((hid == NULL) || (*hid == '\0')) {

		/*
		 * No host specified, so for each host found in the host_list
		 * entries, gather info from the vnodes associated with that host
		 */

		for (i=0; i<host_list_size; ++i) {
			hid = (phost_list+i)->hl_name;
			breturn = build_return_status(bstatus, hid, breturn,
				phost_list, host_list_size,
				consum, consumable_size,
				various);
		}

	} else {

		/*
		 * Specific host names gather info from the vnodes associate with it
		 */

		breturn = build_return_status(bstatus, hid, breturn,
			phost_list, host_list_size,
			consum, consumable_size,
			various);
		if ((breturn == NULL) && (pbs_errno == PBSE_UNKNODE)) {
			/*
			 * store error in TLS if available. Fallback to
			 * connection structure.
			 */
			context = pbs_client_thread_find_connect_context(con);
			if (context != NULL) {
				if (context->th_ch_errtxt != NULL)
					free(context->th_ch_errtxt);
				if ((context->th_ch_errtxt = strdup(pbse_to_txt(pbs_errno))) == NULL) {
					pbs_errno = PBSE_SYSTEM;
					return NULL;
				}
			} else {
				if (connection[con].ch_errtxt != NULL)
					free(connection[con].ch_errtxt);
				if ((connection[con].ch_errtxt = strdup(pbse_to_txt(pbs_errno))) == NULL) {
					pbs_errno = PBSE_SYSTEM;
					return NULL;
				}
			}
		}
	}

	pbs_statfree(bstatus);	/* free info returned by pbs_statvnodes() */
	free(consum);
	consum = NULL;
	consumable_size = 0;
	free(phost_list);
	phost_list = NULL;
	host_list_size  = 0;
	return breturn;
}
