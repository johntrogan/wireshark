/* tap_sipstat.c
 * sip message counter for wireshark
 *
 * Copied from ui/gtk/sip_stat.c and tap-httpstat.c
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <epan/packet_info.h>
#include <epan/tap.h>
#include <epan/stat_tap_ui.h>
#include <epan/value_string.h>
#include <epan/dissectors/packet-sip.h>

#include <wsutil/wslog.h>

#include <wsutil/cmdarg_err.h>

void register_tap_listener_sipstat(void);

/* used to keep track of the statictics for an entire program interface */
typedef struct _sip_stats_t {
	char	    *filter;
	uint32_t	     packets;	 /* number of sip packets, including continuations */
	uint32_t	     resent_packets;
	uint32_t	     average_setup_time;
	uint32_t	     max_setup_time;
	uint32_t	     min_setup_time;
	uint32_t	     no_of_completed_calls;
	uint64_t	     total_setup_time;
	GHashTable  *hash_responses;
	GHashTable  *hash_requests;
} sipstat_t;

/* used to keep track of the stats for a specific response code
 * for example it can be { 3, 404, "Not Found" ,...}
 * which means we captured 3 reply sip/1.1 404 Not Found */
typedef struct _sip_response_code_t {
	uint32_t	     packets;       /* 3 */
	unsigned	     response_code; /* 404 */
	const char *name;	    /* Not Found */
	sipstat_t   *sp;
} sip_response_code_t;

/* used to keep track of the stats for a specific request string */
typedef struct _sip_request_method_t {
	char	    *response;	/* eg. : INVITE */
	uint32_t	     packets;
	sipstat_t   *sp;
} sip_request_method_t;


/* Create tables for responses and requests */
static void
sip_init_hash(sipstat_t *sp)
{
	int i;

	/* Create responses table */
	sp->hash_responses = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, g_free);

	/* Add all response codes */
	for (i=0; sip_response_code_vals[i].strptr; i++)
	{
		int *key = g_new (int, 1);
		sip_response_code_t *sc = g_new (sip_response_code_t, 1);
		*key = sip_response_code_vals[i].value;
		sc->packets = 0;
		sc->response_code =  *key;
		sc->name = sip_response_code_vals[i].strptr;
		sc->sp = sp;
		g_hash_table_insert(sc->sp->hash_responses, key, sc);
	}

	/* Create empty requests table
	 * NB. the sip_request_method_t value is created in sipstat_packet()
	 * with its response member pointing at the same string as was allocated for the key.
	 * Thus it needs no special memory handling, it's freed when GLib frees the key.
	 */
	sp->hash_requests = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

static void
sip_draw_hash_requests( char *key _U_, sip_request_method_t *data, char *format)
{
	if (data->packets == 0)
		return;
	printf( format, data->response, data->packets);
}

static void
sip_draw_hash_responses( int *key _U_ , sip_response_code_t *data, char *format)
{
	if ((data == NULL) || (data->packets == 0))
		return;
	printf(format,  data->response_code, data->name, data->packets );
}

/* NOT USED at this moment */
/*
static void
sip_free_hash( void *key, void *value, void *user_data _U_ )
{
	g_free(key);
	g_free(value);
}
*/

static void
sip_reset_hash_responses(char *key _U_ , sip_response_code_t *data, void *ptr _U_ )
{
	data->packets = 0;
}
static void
sip_reset_hash_requests(char *key _U_ , sip_request_method_t *data, void *ptr _U_ )
{
	data->packets = 0;
}

static void
sipstat_reset(void *psp)
{
	sipstat_t *sp = (sipstat_t *)psp;
	if (sp) {
		sp->packets = 0;
		sp->resent_packets = 0;
		sp->average_setup_time = 0;
		sp->max_setup_time = 0;
		sp->min_setup_time = 0;
		sp->no_of_completed_calls = 0;
		sp->total_setup_time = 0;

		g_hash_table_foreach( sp->hash_responses, (GHFunc)sip_reset_hash_responses, NULL);
		g_hash_table_foreach( sp->hash_requests, (GHFunc)sip_reset_hash_requests, NULL);
	}
}

static void
sipstat_finish(void *psp)
{
	sipstat_t *sp = (sipstat_t *)psp;
	g_free(sp->filter);
	g_hash_table_destroy(sp->hash_responses);
	g_hash_table_destroy(sp->hash_requests);
	g_free(sp);
}

/* Main entry point to SIP tap */
static tap_packet_status
sipstat_packet(void *psp, packet_info *pinfo _U_, epan_dissect_t *edt _U_, const void *pri, tap_flags_t flags _U_)
{
	const sip_info_value_t *value = (const sip_info_value_t *)pri;
	sipstat_t *sp = (sipstat_t *)psp;

	/* Total number of packets, including continuation packets */
	sp->packets++;

	/* Calculate average setup time */
	if (value->setup_time) {
		sp->no_of_completed_calls++;
		/* Check if it's the first value */
		if ( sp->total_setup_time == 0 ) {
			sp->average_setup_time = value->setup_time;
			sp->total_setup_time = value->setup_time;
			sp->max_setup_time = value->setup_time;
			sp->min_setup_time = value->setup_time;
		}else{
			sp->total_setup_time = sp->total_setup_time + value->setup_time;
			if (sp->max_setup_time < value->setup_time) {
				sp->max_setup_time = value->setup_time;
			}
			if (sp->min_setup_time > value->setup_time) {
				sp->min_setup_time = value->setup_time;
			}
			/* Calculate average */
			sp->average_setup_time = (uint32_t)(sp->total_setup_time / sp->no_of_completed_calls);
		}
	}

	/* Update resent count if flag set */
	if (value->resend)
	{
		sp->resent_packets++;
	}


	/* Looking at both requests and responses */
	if (value->response_code != 0)
	{
		/* Responses */
		unsigned key;
		sip_response_code_t *sc;

		/* Look up response code in hash table */
		key = value->response_code;
		sc = (sip_response_code_t *)g_hash_table_lookup(sp->hash_responses, &key);
		if (sc == NULL)
		{
			/* Non-standard status code ; we classify it as others
			 * in the relevant category
			 * (Informational,Success,Redirection,Client Error,Server Error,Global Failure)
			 */
			int i = value->response_code;
			if ((i < 100) || (i >= 700))
			{
				/* Forget about crazy values */
				return TAP_PACKET_DONT_REDRAW;
			}
			else if (i < 200)
			{
				key = 199;	/* Hopefully, this status code will never be used */
			}
			else if (i < 300)
			{
				key = 299;
			}
			else if (i < 400)
			{
				key = 399;
			}
			else if (i < 500)
			{
				key = 499;
			}
			else if (i < 600)
			{
				key = 599;
			}
			else
			{
				key = 699;
			}

			/* Now look up this fallback code to get its text description */
			sc = (sip_response_code_t *)g_hash_table_lookup(sp->hash_responses, &key);
			if (sc == NULL)
			{
				return TAP_PACKET_DONT_REDRAW;
			}
		}
		sc->packets++;
	}
	else if (value->request_method)
	{
		/* Requests */
		sip_request_method_t *sc;

		/* Look up the request method in the table */
		sc = (sip_request_method_t *)g_hash_table_lookup(sp->hash_requests, value->request_method);
		if (sc == NULL)
		{
			/* First of this type. Create structure and initialise */
			sc = g_new(sip_request_method_t, 1);
			sc->response = g_strdup(value->request_method);
			sc->packets = 1;
			sc->sp = sp;
			/* Insert it into request table */
			g_hash_table_insert(sp->hash_requests, sc->response, sc);
		}
		else
		{
			/* Already existed, just update count for that method */
			sc->packets++;
		}
		/* g_free(value->request_method); */
	}
	else
	{
		/* No request method set. Just ignore */
		return TAP_PACKET_DONT_REDRAW;
	}

	return TAP_PACKET_REDRAW;
}

static void
sipstat_draw(void *psp  )
{
	sipstat_t *sp = (sipstat_t *)psp;
	printf("\n");
	printf("===================================================================\n");
	if (sp->filter == NULL)
		printf("SIP Statistics\n");
	else
		printf("SIP Statistics with filter %s\n", sp->filter);

	printf("\nNumber of SIP messages: %u", sp->packets);
	printf("\nNumber of resent SIP messages: %u\n", sp->resent_packets);
	printf(	"\n* SIP Status Codes in reply packets\n");
	g_hash_table_foreach(sp->hash_responses, (GHFunc)sip_draw_hash_responses,
		(void *)"  SIP %3d %-15s : %5d Packets\n");
	printf("\n* List of SIP Request methods\n");
	g_hash_table_foreach(sp->hash_requests,  (GHFunc)sip_draw_hash_requests,
		(void *)"  %-15s : %5d Packets\n");
	printf(	"\n* Average setup time %u ms\n Min %u ms\n Max %u ms\n", sp->average_setup_time, sp->min_setup_time, sp->max_setup_time);
	printf("===================================================================\n");
}

static bool
sipstat_init(const char *opt_arg, void *userdata _U_)
{
	sipstat_t  *sp;
	const char *filter = NULL;
	GString	   *error_string;

	if (strncmp (opt_arg, "sip,stat,", 9) == 0) {
		filter = opt_arg+9;
	} else {
		filter = NULL;
	}

	sp = g_new0(sipstat_t, 1);
	sp->filter = g_strdup(filter);
	/*g_hash_table_foreach( sip_status, (GHFunc)sip_reset_hash_responses, NULL);*/


	error_string = register_tap_listener(
			"sip",
			sp,
			filter,
			TL_REQUIRES_NOTHING,
			sipstat_reset,
			sipstat_packet,
			sipstat_draw,
			sipstat_finish);
	if (error_string) {
		/* error, we failed to attach to the tap. clean up */
		g_free(sp->filter);
		g_free(sp);
		cmdarg_err("Couldn't register sip,stat tap: %s",
			 error_string->str);
		g_string_free(error_string, TRUE);
		return false;
	}

	sp->packets = 0;
	sp->resent_packets = 0;
	sip_init_hash(sp);
	return true;
}

static stat_tap_ui sipstat_ui = {
	REGISTER_STAT_GROUP_GENERIC,
	NULL,
	"sip,stat",
	sipstat_init,
	0,
	NULL
};

void
register_tap_listener_sipstat(void)
{
	register_stat_tap_ui(&sipstat_ui, NULL);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
