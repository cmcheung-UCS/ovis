/* -*- c-basic-offset: 8 -*-
 * Copyright (c) 2017 Sandia Corporation. All rights reserved.
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Export of this program may require a license from the United States
 * Government.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the BSD-type
 * license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *      Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *      Neither the name of Sandia nor the names of any contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *      Neither the name of Open Grid Computing nor the names of any
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *      Modified source versions must be plainly marked as such, and
 *      must not be misrepresented as being the original software.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * \file variable.c
 * \brief 'variable set' test data provider
 */
#define _GNU_SOURCE
#include <inttypes.h>
#include <unistd.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include "ldms.h"
#include "ldmsd.h"

static ldms_set_t set = NULL;
static ldmsd_msg_log_f msglog;
static char *producer_name;
static ldms_schema_t schema;
#define SAMP "variable"
static char *default_schema_name = SAMP;
static uint64_t compid;

static int metric_offset = 1; /* compid is metric 0 */

static char *schema_name;
static char *instance_name;

static int sameticks = 4;
static int curtick = 0;
static int maxmets = 9;
static int minmets = 1;
static int curmets = 1;
static const char *namebase = "metnum";
static uint64_t mval = 0;

static int create_metric_set(const char *in, char* sn)
{
	int rc, i;
	union ldms_value v;
#define NMSZ 128
	char metric_name[NMSZ];

	msglog(LDMSD_LDEBUG, SAMP ": creating set at %" PRIu64 "\n", mval);
	if (sn) {
		schema_name = strdup(sn);
		if (!schema_name) {
			msglog(LDMSD_LERROR, SAMP ": missing schema_name\n");
			rc = ENOMEM;
			goto err;
		}
	}
	schema = ldms_schema_new(schema_name);
	if (!schema) {
		msglog(LDMSD_LERROR, SAMP ": schema_new fail\n");
		rc = ENOMEM;
		goto err;
	}
	if (in) {
		instance_name = strdup(in);
		if (!instance_name) {
			msglog(LDMSD_LERROR, SAMP ": missing instance_name\n");
			rc = ENOMEM;
			goto err;
		}
	}

	rc = ldms_schema_meta_add(schema, "component_id", LDMS_V_U64);
	if (rc < 0) {
		msglog(LDMSD_LERROR, SAMP ": can't add component_id\n");
		rc = ENOMEM;
		goto err;
	}

	i = 0;
	while (i < curmets) {
		snprintf(metric_name, NMSZ, "%s%d", namebase, i);
		msglog(LDMSD_LDEBUG, SAMP ": add metric %s\n", metric_name);
		rc = ldms_schema_metric_add(schema, metric_name, LDMS_V_U64);
		if (rc < 0) {
			msglog(LDMSD_LERROR, SAMP ": failed metric add %s\n",
				metric_name);
			rc = ENOMEM;
			goto err;
		}
		i++;
	}

	set = ldms_set_new(instance_name, schema);
	if (!set) {
		rc = errno;
		msglog(LDMSD_LERROR, SAMP ": can't create set\n");
		goto err;
	}

	/* add specialized metrics */
	v.v_u64 = compid;
	ldms_metric_set(set, 0, &v);
	/* add regular metrics */
	i = 0;
	while (i < curmets) {
		v.v_u64 = mval;
		mval++;
		ldms_metric_set(set, i + metric_offset, &v);
		i++;
	}
	msglog(LDMSD_LDEBUG, SAMP ": created set at %" PRIu64 "\n", mval);
	return 0;

 err:
	if (schema)
		ldms_schema_delete(schema);
	schema = NULL;
	if (schema_name) {
		free(schema_name);
		schema_name = NULL;
	}
	if (instance_name) {
		free(instance_name);
		instance_name = NULL;
	}
	return rc;
}

/**
 * check for invalid flags, with particular emphasis on warning the user about
 */
static int config_check(struct attr_value_list *kwl, struct attr_value_list *avl, void *arg)
{
	char *value;
	int i;

	char* deprecated[]={"set"};

	for (i = 0; i < (sizeof(deprecated)/sizeof(deprecated[0])); i++){
		value = av_value(avl, deprecated[i]);
		if (value){
			msglog(LDMSD_LERROR, SAMP ": config argument %s has been deprecated.\n",
			       deprecated[i]);
			return EINVAL;
		}
	}

	return 0;
}

static const char *usage(struct ldmsd_plugin *self)
{
	return  "config name=" SAMP " producer=<prod_name> instance=<inst_name> [component_id=<compid> schema=<sname> with_jobid=<jid>]\n"
		"    <prod_name>  The producer name\n"
		"    <inst_name>  The instance name\n"
		"    <compid>     Optional unique number identifier. Defaults to zero.\n"
		"    <sname>      Optional schema name. Defaults to '" SAMP "'\n";
}

/**
 * \brief Configuration
 *
 * config name=meminfo producer=<name> instance=<instance_name> [component_id=<compid> schema=<sname>] [with_jobid=<jid>]
 *     producer_name    The producer id value.
 *     instance_name    The set name.
 *     component_id     The component id. Defaults to zero
 *     sname            Optional schema name. Defaults to meminfo
 *     jid              lookup jobid or report 0.
 */
static int config(struct ldmsd_plugin *self, struct attr_value_list *kwl, struct attr_value_list *avl)
{
	char *value;
	char *sname;
	void * arg = NULL;
	int rc;

	rc = config_check(kwl, avl, arg);
	if (rc != 0){
		return rc;
	}

	producer_name = av_value(avl, "producer");
	if (!producer_name) {
		msglog(LDMSD_LERROR, SAMP ": missing producer.\n");
		return ENOENT;
	}
	producer_name = strdup(producer_name);
	if (!producer_name) {
		msglog(LDMSD_LERROR, SAMP ": OOM.\n");
		return ENOMEM;
	}

	value = av_value(avl, "component_id");
	if (value)
		compid = (uint64_t)(atoi(value));
	else
		compid = 0;

	value = av_value(avl, "instance");
	if (!value) {
		msglog(LDMSD_LERROR, SAMP ": missing instance.\n");
		return ENOENT;
	}

	sname = av_value(avl, "schema");
	if (!sname)
		sname = default_schema_name;
	if (strlen(sname) == 0) {
		msglog(LDMSD_LERROR, SAMP ": schema name invalid.\n");
		return EINVAL;
	}

	if (set) {
		msglog(LDMSD_LERROR, SAMP ": Set already created.\n");
		return EINVAL;
	}

	rc = create_metric_set(value, sname);
	if (rc) {
		msglog(LDMSD_LERROR, SAMP ": failed to create a metric set.\n");
		return rc;
	}
	ldms_set_producer_name_set(set, producer_name);
	return 0;
}

static ldms_set_t get_set(struct ldmsd_sampler *self)
{
	return set;
}

static int sample(struct ldmsd_sampler *self)
{
	int rc;
	union ldms_value v;

	if (!set) {
		msglog(LDMSD_LDEBUG, SAMP ": plugin not initialized\n");
		return EINVAL;
	}
	curtick++;
	if (sameticks == curtick) {
		msglog(LDMSD_LDEBUG, SAMP ": set needs update\n");
		curtick = 0;
		curmets++;
		if (curmets > maxmets) {
			curmets = minmets;
		}
		if (set)
			ldms_set_delete(set);
		set = NULL;
		if (schema)
			ldms_schema_delete(schema);
		schema = NULL;
		rc = create_metric_set(NULL, NULL);
		if (rc) {
			msglog(LDMSD_LERROR, SAMP
				": failed to recreate a metric set.\n");
			return rc;
		}
		ldms_set_producer_name_set(set, producer_name);
	}

	ldms_transaction_begin(set);

	int i;
	for (i = 0; i < curmets; i++) {
		v.v_u64 = mval;
		mval++;
		msglog(LDMSD_LDEBUG, SAMP ": setting %d of %d\n",
			i + metric_offset, curmets);
		ldms_metric_set(set, i + metric_offset, &v);
	}

	ldms_transaction_end(set);
	return 0;
}

static void term(struct ldmsd_plugin *self)
{
	if (set)
		ldms_set_delete(set);
	set = NULL;
	if (schema)
		ldms_schema_delete(schema);
	schema = NULL;
	free(schema_name);
	free(producer_name);
}

static struct ldmsd_sampler meminfo_plugin = {
	.base = {
		.name = SAMP,
		.type = LDMSD_PLUGIN_SAMPLER,
		.term = term,
		.config = config,
		.usage = usage,
	},
	.get_set = get_set,
	.sample = sample,
};

struct ldmsd_plugin *get_plugin(ldmsd_msg_log_f pf)
{
	msglog = pf;
	set = NULL;
	return &meminfo_plugin.base;
}
