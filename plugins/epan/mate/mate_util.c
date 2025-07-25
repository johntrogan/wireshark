/* mate_util.c
 * MATE -- Meta Analysis Tracing Engine
 * Utility Library: Single Copy Strings and Attribute Value Pairs
 *
 * Copyright 2004, Luis E. Garcia Ontanon <luis@ontanon.org>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "mate.h"
#include "mate_util.h"

#include <errno.h>
#include <wsutil/file_util.h>


/***************************************************************************
*  dbg_print
***************************************************************************
* This is the debug facility of the thing.
***************************************************************************/

/* dbg_print:
 * which:  a pointer to the current level of debugging for a feature
 * how: the level over which this message should be printed out
 * where: the file on which to print (ws_message if null)
 * fmt, ...: what to print
 */

void dbg_print(const int* which, int how, FILE* where, const char* fmt, ... ) {
	static char debug_buffer[DEBUG_BUFFER_SIZE];
	va_list list;

	if ( ! which || *which < how ) return;

	va_start( list, fmt );
	vsnprintf(debug_buffer,DEBUG_BUFFER_SIZE,fmt,list);
	va_end( list );

	if (! where) {
		ws_message("%s", debug_buffer);
	} else {
		fputs(debug_buffer,where);
		fputs("\n",where);
	}

}

/***************************************************************************
 *  single copy strings
 ***************************************************************************
 * Strings repeat more often than don't. In order to save memory
 * we'll keep only one copy of each as key to a hash with a count of
 * subscribers as value. Each string is truncated to be no more than
 * SCS_HUGE_SIZE.
 * XXX - GLib 2.58 adds reference counted strings, use that?
 ***************************************************************************/

/**
 * scs_init:
 *
 *  Initializes the scs hash.
 **/

struct _scs_collection {
	GHashTable* hash;	/* key: a string value: unsigned number of subscribers */
};

/*
 * XXX: AFAIKT destroy_scs_collection() might be called only when reading a
 *      mate config file. Since reading a new config file can apparently currently
 *      only be done once after starting Wireshark, in theory this fcn
 *      currently should never be called since there will never be an existing
 *      scs_collection to be destroyed.
 */
static void destroy_scs_collection(SCS_collection* c) {
	if (c->hash) g_hash_table_destroy(c->hash);
}

/* The same djb2 hash used in g_str_hash, except that it stops after
 * SCS_HUGE_SIZE bytes. */
static unsigned scs_hash(const void *v)
{
	const signed char *p;
	uint32_t h = 5381;
	size_t i = 0;

	for (p = v; *p != '\0' && i++ < SCS_HUGE_SIZE; p++) {
		h = (h << 5) + h + *p;
	}

	return h;
}

/* Compare the first SCS_HUGE_SIZE bytes for equality. */
static gboolean scs_equal(const void *a, const void *b)
{
	const char *str1 = a;
	const char *str2 = b;

	return strncmp(str1, str2, SCS_HUGE_SIZE) == 0;
}

static SCS_collection* scs_init(void) {
	SCS_collection* c = g_new(SCS_collection, 1);

	c->hash = g_hash_table_new_full(scs_hash, scs_equal, g_free, g_free);

	return c;
}


/**
 * subscribe:
 * @param c the scs hash
 * @param s a string
 *
 * Checks if the given string exists already and if so it increases the count of
 * subsscribers and returns a pointer to the stored string. If not it will copy
 * the given string, store it in the hash, and return the pointer to the copy.
 * Remember, containment is handled internally, take care of your own strings.
 *
 * Return value: a pointer to the subscribed string.
 **/
char* scs_subscribe(SCS_collection* c, const char* s) {
	char* orig = NULL;
	unsigned* ip = NULL;
	size_t len = 0;

	g_hash_table_lookup_extended(c->hash,(const void *)s,(void * *)&orig,(void * *)&ip);

	if (ip) {
		(*ip)++;
	} else {
		ip = g_new0(unsigned, 1);

		len = strlen(s);

		if(G_LIKELY(len <= SCS_HUGE_SIZE)) {
			orig = g_strdup(s);
		} else {
			ws_warning("mate SCS: string truncated due to huge size");
			orig = g_strndup(s, SCS_HUGE_SIZE);
		}

		g_hash_table_insert(c->hash, orig, ip);
	}

	return orig;
}

/**
 * unsubscribe:
 * @param c the scs hash
 * @param s a string.
 *
 * decreases the count of subscribers, if zero frees the internal copy of
 * the string.
 **/
void scs_unsubscribe(SCS_collection* c, char* s) {
	char* orig = NULL;
	unsigned* ip = NULL;

	if (g_hash_table_lookup_extended(c->hash,(const void *)s,(void **)&orig,(void **)&ip)) {
		if (*ip == 0) {
			g_hash_table_remove(c->hash,orig);
		}
		else {
			(*ip)--;
		}
	} else {
		ws_warning("mate SCS: not subscribed");
	}
}

/**
 * scs_subscribe_printf:
 * @param fmt a format string ...
 *
 * Formats the input and subscribes it.
 *
 * Return value: the stored copy of the formatted string.
 *
 **/
char* scs_subscribe_printf(SCS_collection* c, char* fmt, ...) {
	va_list list;
	char *buf, *ret;

	va_start( list, fmt );
	buf = g_strdup_vprintf(fmt, list);
	va_end( list );

	ret = scs_subscribe(c, buf);
	g_free(buf);

	return ret;
}

/***************************************************************************
*  AVPs & Co.
***************************************************************************
* The Thing operates mainly on avps, avpls and loals
* - attribute value pairs (two strings: the name and the value and an operator)
* - avp lists a somehow sorted list of avps
* - loal (list of avp lists) an arbitrarily sorted list of avpls
*
*
***************************************************************************/


typedef union _any_avp_type {
	AVP avp;
	AVPN avpn;
	AVPL avpl;
	LoAL loal;
	LoALnode loaln;
} any_avp_type;


static SCS_collection* avp_strings;

#ifdef _AVP_DEBUGGING
static FILE* dbg_fp;

static int dbg_level;
static int* dbg = &dbg_level;

static int dbg_avp_level;
static int* dbg_avp = &dbg_avp_level;

static int dbg_avp_op_level;
static int* dbg_avp_op = &dbg_avp_op_level;

static int dbg_avpl_level;
static int* dbg_avpl = &dbg_avpl_level;

static int dbg_avpl_op_level;
static int* dbg_avpl_op = &dbg_avpl_op_level;

/**
 * setup_avp_debug:
 * @param fp the file in which to send debugging output.
 * @param general a pointer to the level of debugging of facility "general"
 * @param avp a pointer to the level of debugging of facility "avp"
 * @param avp_op a pointer to the level of debugging of facility "avp_op"
 * @param avpl a pointer to the level of debugging of facility "avpl"
 * @param avpl_op a pointer to the level of debugging of facility "avpl_op"
 *
 * If enabled sets up the debug facilities for the avp library.
 *
 **/
extern void setup_avp_debug(FILE* fp, int* general, int* avp, int* avp_op, int* avpl, int* avpl_op) {
	dbg_fp = fp;
	dbg = general;
	dbg_avp = avp;
	dbg_avp_op = avp_op;
	dbg_avpl = avpl;
	dbg_avpl_op = avpl_op;
}

#endif /* _AVP_DEBUGGING */

/**
 * avp_init:
 *
 * (Re)Initializes the avp library.
 *
 **/
extern void avp_init(void) {

	if (avp_strings) destroy_scs_collection(avp_strings);
	avp_strings = scs_init();

}

/**
 * new_avp_from_finfo:
 * @param name the name the avp will have.
 * @param finfo the field_info from which to fetch the data.
 *
 * Creates an avp from a field_info record.
 *
 * Return value: a pointer to the newly created avp.
 *
 **/
extern AVP* new_avp_from_finfo(const char* name, field_info* finfo) {
	AVP*   new_avp_val = (AVP*)g_slice_new(any_avp_type);
	char* value;
	char* repr;

	new_avp_val->n = scs_subscribe(avp_strings, name);

	repr = fvalue_to_string_repr(NULL, finfo->value, FTREPR_DISPLAY, finfo->hfinfo->display);

	if (repr) {
		value = scs_subscribe(avp_strings, repr);
		wmem_free(NULL, repr);
#ifdef _AVP_DEBUGGING
		dbg_print (dbg_avp,2,dbg_fp,"new_avp_from_finfo: from string: %s",value);
#endif
	} else {
#ifdef _AVP_DEBUGGING
		dbg_print (dbg_avp,2,dbg_fp,"new_avp_from_finfo: a proto: %s",finfo->hfinfo->abbrev);
#endif
		value = scs_subscribe(avp_strings, "");
	}

	new_avp_val->v = value;

	new_avp_val->o = '=';

#ifdef _AVP_DEBUGGING
	dbg_print (dbg_avp,1,dbg_fp,"new_avp_from_finfo: %p %s%c%s;",new_avp_val,new_avp_val->n,new_avp_val->o,new_avp_val->v);
#endif

	return new_avp_val;
}


/**
 * new_avp:
 * @param name the name the avp will have.
 * @param value the value the avp will have.
 * @param o the operator of this avp.
 *
 * Creates an avp given every parameter.
 *
 * Return value: a pointer to the newly created avp.
 *
 **/
extern AVP* new_avp(const char* name, const char* value, char o) {
	AVP* new_avp_val = (AVP*)g_slice_new(any_avp_type);

	new_avp_val->n = scs_subscribe(avp_strings, name);
	new_avp_val->v = scs_subscribe(avp_strings, value);
	new_avp_val->o = o;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avp,1,dbg_fp,"new_avp_val: %p %s%c%s;",new_avp_val,new_avp_val->n,new_avp_val->o,new_avp_val->v);
#endif
	return new_avp_val;
}


/**
* delete_avp:
 * @param avp the avp to delete.
 *
 * Destroys an avp and releases the resources it uses.
 *
 **/
extern void delete_avp(AVP* avp) {
#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avp,1,dbg_fp,"delete_avp: %p %s%c%s;",avp,avp->n,avp->o,avp->v);
#endif

	scs_unsubscribe(avp_strings, avp->n);
	scs_unsubscribe(avp_strings, avp->v);
	g_slice_free(any_avp_type,(any_avp_type*)avp);
}


/**
* avp_copy:
 * @param from the avp to be copied.
 *
 * Creates an avp whose name op and value are copies of the given one.
 *
 * Return value: a pointer to the newly created avp.
 *
 **/
extern AVP* avp_copy(AVP* from) {
	AVP* new_avp_val = (AVP*)g_slice_new(any_avp_type);

	new_avp_val->n = scs_subscribe(avp_strings, from->n);
	new_avp_val->v = scs_subscribe(avp_strings, from->v);
	new_avp_val->o = from->o;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avp,1,dbg_fp,"copy_avp: %p %s%c%s;",new_avp_val,new_avp_val->n,new_avp_val->o,new_avp_val->v);
#endif

	return new_avp_val;
}

/**
 * new_avpl:
 * @param name the name the avpl will have.
 *
 * Creates an empty avpl.
 *
 * Return value: a pointer to the newly created avpl.
 *
 **/
extern AVPL* new_avpl(const char* name) {
	AVPL* new_avpl_p = (AVPL*)g_slice_new(any_avp_type);

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,7,dbg_fp,"new_avpl_p: %p name=%s",new_avpl_p,name);
#endif

	new_avpl_p->name = name ? scs_subscribe(avp_strings, name) : scs_subscribe(avp_strings, "");
	new_avpl_p->len = 0;
	new_avpl_p->null.avp = NULL;
	new_avpl_p->null.next = &new_avpl_p->null;
	new_avpl_p->null.prev = &new_avpl_p->null;


	return new_avpl_p;
}

extern void rename_avpl(AVPL* avpl, char* name) {
	scs_unsubscribe(avp_strings,avpl->name);
	avpl->name = scs_subscribe(avp_strings,name);
}

/**
 * insert_avp_before_node:
 * @param avpl the avpl in which to insert.
 * @param next_node the next node before which the new avpn has to be inserted.
 * @param avp the avp to be inserted.
 * @param copy_avp whether the original AVP or a copy thereof must be inserted.
 *
 * Pre-condition: the avp is sorted before before_avp and does not already exist
 * in the avpl.
 */
static void insert_avp_before_node(AVPL* avpl, AVPN* next_node, AVP *avp, bool copy_avp) {
	AVPN* new_avp_val = (AVPN*)g_slice_new(any_avp_type);

	new_avp_val->avp = copy_avp ? avp_copy(avp) : avp;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,7,dbg_fp,"new_avpn: %p",new_avp_val);
	dbg_print(dbg_avpl,5,dbg_fp,"insert_avp:  inserting %p in %p before %p;",avp,avpl,next_node);
#endif

	new_avp_val->next = next_node;
	new_avp_val->prev = next_node->prev;
	next_node->prev->next = new_avp_val;
	next_node->prev = new_avp_val;

	avpl->len++;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl,4,dbg_fp,"avpl: %p new len: %i",avpl,avpl->len);
#endif
}

/**
 * insert_avp:
 * @param avpl the avpl in which to insert.
 * @param avp the avp to be inserted.
 *
 * Inserts the given AVP into the given AVPL if an identical one isn't yet there.
 *
 * Return value: whether it was inserted or not.
 *
 * BEWARE: Check the return value, you might need to delete the avp if
 *         it is not inserted.
 **/
extern bool insert_avp(AVPL* avpl, AVP* avp) {
	AVPN* c;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,4,dbg_fp,"insert_avp: %p %p %s%c%s;",avpl,avp,avp->n,avp->o,avp->v);
#endif

	/* get to the insertion point */
	for (c=avpl->null.next; c->avp; c = c->next) {
		int name_diff = strcmp(avp->n, c->avp->n);

		if (name_diff == 0) {
			int value_diff = strcmp(avp->v, c->avp->v);

			if (value_diff < 0) {
				break;
			}

			if (value_diff == 0) {
				// ignore duplicate values, prevents (a=1, a=1)
				// note that this is also used to insert
				// conditions AVPs, so really check if the name,
				// value and operator are all equal.
				if (c->avp->o == avp->o && avp->o == AVP_OP_EQUAL) {
					return false;
				}
			}
		}

		if (name_diff < 0) {
			break;
		}
	}

	insert_avp_before_node(avpl, c, avp, false);

	return true;
}

/**
 * get_avp_by_name:
 * @param avpl the avpl from which to try to get the avp.
 * @param name the name of the avp we are looking for.
 * @param cookie variable in which to store the state between calls.
 *
 * Gets  pointer to the next avp whose name is given; uses cookie to store its
 * state between calls.
 *
 * Return value: a pointer to the next matching avp if there's one, else NULL.
 *
 **/
extern AVP* get_avp_by_name(AVPL* avpl, char* name, void** cookie) {
	AVPN* curr;
	AVPN* start = (AVPN*) *cookie;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,7,dbg_fp,"get_avp_by_name: entering: %p %s %p",avpl,name,*cookie);
#endif

	name = scs_subscribe(avp_strings, name);

	if (!start) start = avpl->null.next;

	for ( curr = start; curr->avp; curr = curr->next ) {
		if ( curr->avp->n == name ) {
			break;
		}
	}

	*cookie = curr;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,5,dbg_fp,"get_avp_by_name: got avp: %p",curr);
#endif

	scs_unsubscribe(avp_strings, name);

	return curr->avp;
}

/**
 * extract_avp_by_name:
 * @param avpl the avpl from which to try to extract the avp.
 * @param name the name of the avp we are looking for.
 *
 * Extracts from the avpl the next avp whose name is given;
 *
 * Return value: a pointer to extracted avp if there's one, else NULL.
 *
 **/
extern AVP* extract_avp_by_name(AVPL* avpl, char* name) {
	AVPN* curr;
	AVP* avp = NULL;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,7,dbg_fp,"extract_avp_by_name: entering: %p %s",avpl,name);
#endif

	name = scs_subscribe(avp_strings, name);

	for ( curr = avpl->null.next; curr->avp; curr = curr->next ) {
		if ( curr->avp->n == name ) {
			break;
		}
	}

	scs_unsubscribe(avp_strings, name);

	if( ! curr->avp ) return NULL;

	curr->next->prev = curr->prev;
	curr->prev->next = curr->next;

	avp = curr->avp;

	g_slice_free(any_avp_type,(any_avp_type*)curr);

	(avpl->len)--;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl,4,dbg_fp,"avpl: %p new len: %i",avpl,avpl->len);
#endif

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,5,dbg_fp,"extract_avp_by_name: got avp: %p",avp);
#endif

	return avp;
}


/**
 * extract_first_avp:
 * @param avpl the avpl from which to try to extract the avp.
 *
 * Extracts the first avp from the avpl.
 *
 * Return value: a pointer to extracted avp if there's one, else NULL.
 *
 **/
extern AVP* extract_first_avp(AVPL* avpl) {
	AVP* avp;
	AVPN* node;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,7,dbg_fp,"extract_first_avp: %p",avpl);
#endif

	node = avpl->null.next;

	avpl->null.next->prev = &avpl->null;
	avpl->null.next = node->next;

	avp = node->avp;

	if (avp) {
		g_slice_free(any_avp_type,(any_avp_type*)node);
		(avpl->len)--;
#ifdef _AVP_DEBUGGING
		dbg_print(dbg_avpl,4,dbg_fp,"avpl: %p new len: %i",avpl,avpl->len);
#endif
	}

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,5,dbg_fp,"extract_first_avp: got avp: %p",avp);
#endif

	return avp;

}


/**
 * extract_last_avp:
 * @param avpl the avpl from which to try to extract the avp.
 *
 * Extracts the last avp from the avpl.
 *
 * Return value: a pointer to extracted avp if there's one, else NULL.
 *
 **/
extern AVP* extract_last_avp(AVPL* avpl) {
	AVP* avp;
	AVPN* node;

	node = avpl->null.prev;

	avpl->null.prev->next = &avpl->null;
	avpl->null.prev = node->prev;

	avp = node->avp;

	if (avp) {
		g_slice_free(any_avp_type,(any_avp_type*)node);
		(avpl->len)--;
#ifdef _AVP_DEBUGGING
		dbg_print(dbg_avpl,4,dbg_fp,"avpl: %p new len: %i",avpl,avpl->len);
#endif
	}

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,5,dbg_fp,"extract_last_avp: got avp: %p",avp);
#endif

	return avp;

}


/**
 * delete_avpl:
 * @param avpl the avpl from which to try to extract the avp.
 * @param avps_too whether or not it should delete the avps as well.
 *
 * Destroys an avpl and releases the resources it uses. If told to do
 * so releases the avps as well.
 *
 **/
extern void delete_avpl(AVPL* avpl, bool avps_too) {
	AVP* avp;
#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl,3,dbg_fp,"delete_avpl: %p",avpl);
#endif

	while(( avp = extract_last_avp(avpl))) {
		if (avps_too) {
			delete_avp(avp);
		}
	}

	scs_unsubscribe(avp_strings,avpl->name);
	g_slice_free(any_avp_type,(any_avp_type*)avpl);
}



/**
 * get_next_avp:
 * @param avpl the avpl from which to try to get the avps.
 * @param cookie variable in which to store the state between calls.
 *
 * Iterates on an avpl to get its avps.
 *
 * Return value: a pointer to the next avp if there's one, else NULL.
 *
 **/
extern AVP* get_next_avp(AVPL* avpl, void** cookie) {
	AVPN* node;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,5,dbg_fp,"get_next_avp: avpl: %p avpn: %p",avpl,*cookie);
#endif

	if (*cookie) {
		node = (AVPN*) *cookie;
	} else {
		node = avpl->null.next;
	}

	*cookie = node->next;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,5,dbg_fp,"extract_last_avp: got avp: %p",node->avp);
#endif

	return node->avp;
}

/**
 * avpl_to_str:
 * @param avpl the avpl to represent.
 *
 * Creates a newly allocated string containing a representation of an avpl.
 *
 * Return value: a pointer to the newly allocated string.
 *
 **/
char* avpl_to_str(AVPL* avpl) {
	AVPN* c;
	GString* s = g_string_new("");
	char* avp_s;
	char* r;

	for(c=avpl->null.next; c->avp; c = c->next) {
		avp_s = avp_to_str(c->avp);
		g_string_append_printf(s," %s;",avp_s);
		g_free(avp_s);
	}

	r = g_string_free(s,FALSE);

	/* g_strchug(r); ? */
	return r;
}

extern char* avpl_to_dotstr(AVPL* avpl) {
	AVPN* c;
	GString* s = g_string_new("");
	char* avp_s;
	char* r;

	for(c=avpl->null.next; c->avp; c = c->next) {
		avp_s = avp_to_str(c->avp);
		g_string_append_printf(s," .%s;",avp_s);
		g_free(avp_s);
	}

	r = g_string_free(s,FALSE);

	/* g_strchug(r); ? */
	return r;
}

/**
* merge_avpl:
 * @param dst the avpl in which to merge the avps.
 * @param src the avpl from which to get the avps.
 * @param copy_avps whether avps should be copied instead of referenced.
 *
 * Adds the avps of src that are not existent in dst into dst.
 *
 **/
extern void merge_avpl(AVPL* dst, AVPL* src, bool copy_avps) {
	AVPN* cd = NULL;
	AVPN* cs = NULL;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"merge_avpl: %p %p",dst,src);
#endif

	cs = src->null.next;
	cd = dst->null.next;

	while (cs->avp && cd->avp) {

		int name_diff = strcmp(cd->avp->n, cs->avp->n);

		if (name_diff < 0) {
			// dest < source, advance dest to find a better place to insert
			cd = cd->next;
		} else if (name_diff > 0) {
			// dest > source, so it can be definitely inserted here.
			insert_avp_before_node(dst, cd, cs->avp, copy_avps);
			cs = cs->next;
		} else {
			// attribute names are equal. Ignore duplicate values but ensure that other values are sorted.
			int value_diff = strcmp(cd->avp->v, cs->avp->v);

			if (value_diff < 0) {
				// dest < source, do not insert it yet
				cd = cd->next;
			} else if (value_diff > 0) {
				// dest > source, insert AVP before the current dest AVP
				insert_avp_before_node(dst, cd, cs->avp, copy_avps);
				cs = cs->next;
			} else {
				// identical AVPs, do not create a duplicate.
				cs = cs->next;
			}
		}
	}

	// if there are remaining source AVPs while there are no more destination
	// AVPs (cd now represents the NULL item, after the last item), append
	// all remaining source AVPs to the end
	while (cs->avp) {
		insert_avp_before_node(dst, cd, cs->avp, copy_avps);
		cs = cs->next;
	}

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,8,dbg_fp,"merge_avpl: done");
#endif

	return;
}


/**
 * new_avpl_from_avpl:
 * @param name the name of the new avpl.
 * @param avpl the avpl from which to get the avps.
 * @param copy_avps whether avps should be copied instead of referenced.
 *
 * Creates a new avpl containing the same avps as the given avpl
 * It will either reference or copy the avps.
 *
 * Return value: a pointer to the newly allocated string.
 *
 **/
extern AVPL* new_avpl_from_avpl(const char* name, AVPL* avpl, bool copy_avps) {
	AVPL* newavpl = new_avpl(name);
	void* cookie = NULL;
	AVP* avp;
	AVP* copy;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"new_avpl_from_avpl: %p from=%p name='%s'",newavpl,avpl,name);
#endif

	while(( avp = get_next_avp(avpl,&cookie) )) {
		if (copy_avps) {
			copy = avp_copy(avp);
			if ( ! insert_avp(newavpl,copy) ) {
				delete_avp(copy);
			}
		} else {
			insert_avp(newavpl,avp);
		}
	}

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,8,dbg_fp,"new_avpl_from_avpl: done");
#endif

	return newavpl;
}

/**
* match_avp:
 * @param src an src to be compared against an "op" avp
 * @param op the "op" avp that will be matched against the src avp
 *
 * Checks whether or not two avp's match.
 *
 * Return value: a pointer to the src avp if there's a match.
 *
 **/
extern AVP* match_avp(AVP* src, AVP* op) {
	char** splited;
	int i;
	char* p;
	unsigned ls;
	unsigned lo;
	double fs = 0.0;
	double fo = 0.0;
	bool lower = false;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"match_avp: %s%c%s; vs. %s%c%s;",src->n,src->o,src->v,op->n,op->o,op->v);
#endif

	if ( src->n != op->n ) {
		return NULL;
	}

	switch (op->o) {
		case AVP_OP_EXISTS:
			return src;
		case AVP_OP_EQUAL:
			return src->v == op->v ? src : NULL;
		case AVP_OP_NOTEQUAL:
			return !( src->v == op->v) ? src : NULL;
		case AVP_OP_STARTS:
			return strncmp(src->v,op->v,strlen(op->v)) == 0 ? src : NULL;
		case AVP_OP_ONEOFF:
			splited = g_strsplit(op->v,"|",0);
			if (splited) {
				for (i=0;splited[i];i++) {
					if(g_str_equal(splited[i],src->v)) {
						g_strfreev(splited);
						return src;
					}
				}
				g_strfreev(splited);
			}
			return NULL;

		case AVP_OP_LOWER:
			lower = true;
			/* FALLTHRU */
		case AVP_OP_HIGHER:

			fs = g_ascii_strtod(src->v, NULL);
			fo = g_ascii_strtod(op->v, NULL);

			if (lower) {
				if (fs<fo) return src;
				else return NULL;
			} else {
				if (fs>fo) return src;
				else return NULL;
			}
		case AVP_OP_ENDS:
			/* does this work? */
			ls = (unsigned) strlen(src->v);
			lo = (unsigned) strlen(op->v);

			if ( ls < lo ) {
				return NULL;
			} else {
				p = src->v + ( ls - lo );
				return g_str_equal(p,op->v) ? src : NULL;
			}

		/* case AVP_OP_TRANSF: */
		/*	return do_transform(src,op); */
		case AVP_OP_CONTAINS:
			return g_strrstr(src->v, op->v) ? src : NULL;
	}
	/* will never get here */
	return NULL;
}



/**
 * new_avpl_loose_match:
 * @param name the name of the resulting avpl
 * @param src the data AVPL to be matched against a condition AVPL
 * @param op the conditions AVPL that will be matched against the data AVPL
 * @param copy_avps whether the avps in the resulting avpl should be copied
 *
 * Creates a new AVP list containing all data AVPs that matched any of the
 * conditions AVPs. If there are no matches, an empty list will be returned.
 *
 * Note: Loose will always be considered a successful match, it matches zero or
 * more conditions.
 */
extern AVPL* new_avpl_loose_match(const char* name,
								  AVPL* src,
								  AVPL* op,
								  bool copy_avps) {

	AVPL* newavpl = new_avpl(scs_subscribe(avp_strings, name));
	AVPN* co = NULL;
	AVPN* cs = NULL;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"new_avpl_loose_match: %p src=%p op=%p name='%s'",newavpl,src,op,name);
#endif


	cs = src->null.next;
	co = op->null.next;
	while (cs->avp && co->avp) {
		int name_diff = strcmp(co->avp->n, cs->avp->n);

		if (name_diff < 0) {
			// op < source, op is not matching
			co = co->next;
		} else if (name_diff > 0) {
			// op > source, source is not matching
			cs = cs->next;
		} else {
			// attribute match found, let's see if there is any condition (op) that accepts this data AVP.
			AVPN *cond = co;
			do {
				if (match_avp(cs->avp, cond->avp)) {
					insert_avp_before_node(newavpl, newavpl->null.prev->next, cs->avp, copy_avps);
					break;
				}
				cond = cond->next;
			} while (cond->avp && cond->avp->n == cs->avp->n);
			cs = cs->next;
		}
	}

	// return matches (possible none)
	return newavpl;
}

/**
* new_avpl_pairs_match:
 * @param name the name of the resulting avpl
 * @param src the data AVPL to be matched against a condition AVPL
 * @param op the conditions AVPL that will be matched against the data AVPL
 * @param strict true if every condition must have a matching data AVP, false if
 * it is also acceptable that only one of the condition AVPs for the same
 * attribute is matching.
 * @param copy_avps whether the avps in the resulting avpl should be copied
 *
 * Creates an AVP list by matching pairs of conditions and data AVPs, returning
 * the data AVPs. If strict is true, then each condition must be paired with a
 * matching data AVP. If strict is false, then some conditions are allowed to
 * fail when other conditions for the same attribute do have a match. Note that
 * if the condition AVPL is empty, the result will be a match (an empty list).
 *
 * Return value: a pointer to the newly created avpl containing the
 *				 matching avps or NULL if there is no match.
 */
extern AVPL* new_avpl_pairs_match(const char* name, AVPL* src, AVPL* op, bool strict, bool copy_avps) {
	AVPL* newavpl;
	AVPN* co = NULL;
	AVPN* cs = NULL;
	const char *last_match = NULL;
	bool matched = true;

	newavpl = new_avpl(scs_subscribe(avp_strings, name));

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"%s: %p src=%p op=%p name='%s'",G_STRFUNC,newavpl,src,op,name);
#endif

	cs = src->null.next;
	co = op->null.next;
	while (cs->avp && co->avp) {
		int name_diff = g_strcmp0(co->avp->n, cs->avp->n);
		const char *failed_match = NULL;

		if (name_diff < 0) {
			// op < source, op has no data avp with same attribute.
			failed_match = co->avp->n;
			co = co->next;
		} else if (name_diff > 0) {
			// op > source, the source avp is not matched by any condition
			cs = cs->next;
		} else {
			// Matching attributes found, now try to find a matching data AVP for the condition.
			if (match_avp(cs->avp, co->avp)) {
				insert_avp_before_node(newavpl, newavpl->null.prev->next, cs->avp, copy_avps);
				last_match = co->avp->n;
				cs = cs->next;
			} else {
				failed_match = co->avp->n;
			}
			co = co->next;
		}

		// condition did not match, check if we can continue matching.
		if (failed_match) {
			if (strict) {
				matched = false;
				break;
			} else if (last_match != failed_match) {
				// None of the conditions so far matched the attribute, check for other candidates
				if (!co->avp || co->avp->n != last_match) {
					matched = false;
					break;
				}
			}
		}
	}

	// if there are any conditions remaining, then those could not be matched
	if (matched && strict && co->avp) {
		matched = false;
	}

	if (matched) {
		// there was a match, accept it
		return newavpl;
	} else {
		// no match, only delete AVPs too if they were copied
		delete_avpl(newavpl, copy_avps);
		return NULL;
	}
}


/**
 * new_avpl_from_match:
 * @param mode The matching method, one of AVPL_STRICT, AVPL_LOOSE, AVPL_EVERY.
 * @param name the name of the resulting avpl
 * @param src the data AVPL to be matched against a condition AVPL
 * @param op the conditions AVPL that will be matched against the data AVPL
 *
 * Matches the conditions AVPL against the original AVPL according to the mode.
 * If there is no match, NULL is returned. If there is actually a match, then
 * the matching AVPs (a subset of the data) are returned.
 */
extern AVPL* new_avpl_from_match(avpl_match_mode mode, const char* name,AVPL* src, AVPL* op, bool copy_avps) {
	AVPL* avpl = NULL;

	switch (mode) {
		case AVPL_STRICT:
			avpl = new_avpl_pairs_match(name, src, op, true, copy_avps);
			break;
		case AVPL_LOOSE:
			avpl = new_avpl_loose_match(name,src,op,copy_avps);
			break;
		case AVPL_EVERY:
			avpl = new_avpl_pairs_match(name, src, op, false, copy_avps);
			break;
		case AVPL_NO_MATCH:
			// XXX this seems unused
			avpl = new_avpl_from_avpl(name,src,copy_avps);
			merge_avpl(avpl, op, copy_avps);
			break;
	}

	return avpl;
}

/**
 * delete_avpl_transform:
 * @param op a pointer to the avpl transformation object
 *
 * Destroys an avpl transformation object and releases all the resources it
 * uses.
 *
 **/
extern void delete_avpl_transform(AVPL_Transf* op) {
	AVPL_Transf* next;

	for (; op ; op = next) {
		next = op->next;

		g_free(op->name);

		if (op->match) {
			delete_avpl(op->match,true);
		}

		if (op->replace) {
			delete_avpl(op->replace,true);
		}

		g_free(op);
	}

}


/**
 * avpl_transform:
 * @param src the source avpl for the transform operation.
 * @param op a pointer to the avpl transformation object to apply.
 *
 * Applies the "op" transformation to an avpl, matches it and eventually
 * replaces or inserts the transformed avps.
 *
 * Return value: whether the transformation was performed or not.
 **/
extern void avpl_transform(AVPL* src, AVPL_Transf* op) {
	AVPL* avpl = NULL;
	AVPN* cs;
	AVPN* cm;
	AVPN* n;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"avpl_transform: src=%p op=%p",src,op);
#endif

	for ( ; op ; op = op->next) {

		avpl = new_avpl_from_match(op->match_mode, src->name,src, op->match, true);

		if (avpl) {
			switch (op->replace_mode) {
				case AVPL_NO_REPLACE:
					delete_avpl(avpl,true);
					return;
				case AVPL_INSERT:
					merge_avpl(src,op->replace,true);
					delete_avpl(avpl,true);
					return;
				case AVPL_REPLACE:
					cs = src->null.next;
					cm = avpl->null.next;
					// Removes AVPs from the source which  are in the matched data.
					// Assume that the matched set is a subset of the source.
					while (cs->avp && cm->avp) {
						if (cs->avp->n == cm->avp->n && cs->avp->v == cm->avp->v) {
							n = cs->next;

							cs->prev->next = cs->next;
							cs->next->prev = cs->prev;

							if (cs->avp)
								delete_avp(cs->avp);
							g_slice_free(any_avp_type,(any_avp_type*)cs);

							cs = n;
							cm = cm->next;
						} else {
							// Current matched AVP is not equal to the current
							// source AVP. Since there must be a source AVP for
							// each matched AVP, advance current source and not
							// the match AVP.
							cs = cs->next;
						}
					}

					merge_avpl(src,op->replace,true);
					delete_avpl(avpl,true);
					return;
			}
		}
	}
}


/**
 * new_loal:
 * @param name the name the loal will take.
 *
 * Creates an empty list of avp lists.
 *
 * Return value: a pointer to the newly created loal.
 **/
extern LoAL* new_loal(const char* name) {
	LoAL* new_loal_p = (LoAL*)g_slice_new(any_avp_type);

	if (! name) {
		name = "anonymous";
	}

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"new_loal_p: %p name=%s",new_loal_p,name);
#endif

	new_loal_p->name = scs_subscribe(avp_strings,name);
	new_loal_p->null.avpl = NULL;
	new_loal_p->null.next = &new_loal_p->null;
	new_loal_p->null.prev = &new_loal_p->null;
	new_loal_p->len = 0;
	return new_loal_p;
}

/**
 * loal_append:
 * @param loal the loal on which to operate.
 * @param avpl the avpl to append.
 *
 * Appends an avpl to a loal.
 *
 **/
extern void loal_append(LoAL* loal, AVPL* avpl) {
	LoALnode* node = (LoALnode*)g_slice_new(any_avp_type);

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"new_loal_node: %p",node);
#endif

	node->avpl = avpl;
	node->next = &loal->null;
	node->prev = loal->null.prev;

	loal->null.prev->next = node;
	loal->null.prev = node;
	loal->len++;
}


/**
 * extract_first_avpl:
 * @param loal the loal on which to operate.
 *
 * Extracts the first avpl contained in a loal.
 *
 * Return value: a pointer to the extracted avpl.
 *
 **/
extern AVPL* extract_first_avpl(LoAL* loal) {
	LoALnode* node;
	AVPL* avpl;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"extract_first_avpl: from: %s",loal->name);
#endif

	node = loal->null.next;

	loal->null.next->next->prev = &loal->null;
	loal->null.next = node->next;

	loal->len--;

	avpl = node->avpl;

	if ( avpl ) {
		g_slice_free(any_avp_type,(any_avp_type*)node);

#ifdef _AVP_DEBUGGING
		dbg_print(dbg_avpl_op,3,dbg_fp,"extract_first_avpl: got %s",avpl->name);
		dbg_print(dbg_avpl_op,3,dbg_fp,"delete_loal_node: %p",node);
#endif
	}

	return avpl;
}

/**
* extract_first_avpl:
 * @param loal the loal on which to operate.
 *
 * Extracts the last avpl contained in a loal.
 *
 * Return value: a pointer to the extracted avpl.
 *
 **/
extern AVPL* extract_last_avpl(LoAL* loal){
	LoALnode* node;
	AVPL* avpl;

	node = loal->null.prev;

	loal->null.prev->prev->next = &loal->null;
	loal->null.prev = node->prev;

	loal->len--;

	avpl = node->avpl;

	if ( avpl ) {
		g_slice_free(any_avp_type,(any_avp_type*)node);
#ifdef _AVP_DEBUGGING
		dbg_print(dbg_avpl_op,3,dbg_fp,"delete_loal_node: %p",node);
#endif
	}

	return avpl;
}

/**
 * extract_first_avpl:
 * @param loal the loal on which to operate.
 * @param cookie pointer to the pointer variable to contain the state between calls
 *
 * At each call will return the following avpl from a loal. The given cookie
 * will be used to manatain the state between calls.
 *
 * Return value: a pointer to the next avpl.
 *
 **/
extern AVPL* get_next_avpl(LoAL* loal,void** cookie) {
	LoALnode* node;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"get_next_avpl: loal=%p node=%p",loal,*cookie);
#endif

	if (*cookie) {
		node = (LoALnode*) *cookie;
	} else {
		node = loal->null.next;
	}

	*cookie = node->next;

	return node->avpl;
}

/**
 * delete_loal:
 * @param loal the loal to be deleted.
 * @param avpls_too whether avpls contained by the loal should be deleted as well
 * @param avps_too whether avps contained by the avpls should be also deleted
 *
 * Destroys a loal and eventually desstroys avpls and avps.
 *
 **/
extern void delete_loal(LoAL* loal, bool avpls_too, bool avps_too) {
	AVPL* avpl;

#ifdef _AVP_DEBUGGING
	dbg_print(dbg_avpl_op,3,dbg_fp,"delete_loal: %p",loal);
#endif

	while(( avpl = extract_last_avpl(loal) )) {
		if (avpls_too) {
			delete_avpl(avpl,avps_too);
		}
	}

	scs_unsubscribe(avp_strings,loal->name);
	g_slice_free(any_avp_type,(any_avp_type*)loal);
}



/****************************************************************************
 ******************* the following are used in load_loal_from_file
 ****************************************************************************/

/**
 * load_loal_error:
 * Used by loal_from_file to handle errors while loading.
 **/
static LoAL* load_loal_error(FILE* fp, LoAL* loal, AVPL* curr, int linenum, const char* fmt, ...) {
	va_list list;
	char* desc;
	LoAL* ret = NULL;
	char* err;

	va_start( list, fmt );
	desc = ws_strdup_vprintf(fmt, list);
	va_end( list );

	if (loal) {
		err = ws_strdup_printf("Error Loading LoAL from file: in %s at line: %i, %s",loal->name,linenum,desc);
	} else {
		err = ws_strdup_printf("Error Loading LoAL at line: %i, %s",linenum,desc);
	}
	ret = new_loal(err);

	g_free(desc);
	g_free(err);

	if (fp) fclose(fp);
	if (loal) delete_loal(loal,true,true);
	if (curr) delete_avpl(curr,true);

	return ret;
}


/*  the maximum length allowed for a line */
#define MAX_ITEM_LEN	8192

/* this two ugly things are used for tokenizing */
#define AVP_OP_CHAR '=': case '^': case '$': case '~': case '<': case '>': case '?': case '|': case '&' : case '!'

#define AVP_NAME_CHAR 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':\
case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':\
case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z': case 'a': case 'b': case 'c': case 'd':\
case 'e': case 'f': case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':\
case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x':\
case 'y': case 'z': case '_': case '0': case '1': case '2': case '3': case '4': case '5': case '6':\
case '7': case '8': case '9': case '.'


/**
 * loal_from_file:
 * @param filename the file containing a loals text representation.
 *
 * Given a filename it will attempt to load a loal containing a copy of
 * the avpls represented in the file.
 *
 * Return value: if successful a pointer to the new populated loal, else NULL.
 *
 **/
extern LoAL* loal_from_file(char* filename) {
	FILE *fp = NULL;
	char c;
	int i = 0;
	uint32_t linenum = 1;
	char *linenum_buf;
	char *name;
	char *value;
	char op = '?';
	LoAL *loal_error, *loal = new_loal(filename);
	AVPL* curr = NULL;
	AVP* avp;

	enum _load_loal_states {
		START,
		BEFORE_NAME,
		IN_NAME,
		IN_VALUE,
		MY_IGNORE
	} state;

	linenum_buf = (char*)g_malloc(MAX_ITEM_LEN);
	name = (char*)g_malloc(MAX_ITEM_LEN);
	value = (char*)g_malloc(MAX_ITEM_LEN);
#ifndef _WIN32
	if (! getuid()) {
		loal_error = load_loal_error(fp,loal,curr,linenum,"MATE Will not run as root");
		goto error;
	}
#endif

	state = START;

	if (( fp = ws_fopen(filename,"r") )) {
		while(( c = (char) fgetc(fp) )){

			if ( feof(fp) ) {
				if ( ferror(fp) ) {
					report_read_failure(filename,errno);
					loal_error = load_loal_error(fp,loal,curr,linenum,"Error while reading '%f'",filename);
					goto error;
				}
				break;
			}

			if ( c == '\n' ) {
				linenum++;
			}

			if ( i >= MAX_ITEM_LEN - 1  ) {
				loal_error = load_loal_error(fp,loal,curr,linenum,"Maximum item length exceeded");
				goto error;
			}

			switch(state) {
				case MY_IGNORE:
					switch (c) {
						case '\n':
							state = START;
							i = 0;
							continue;
						default:
							continue;
					}
				case START:
					switch (c) {
						case ' ': case '\t':
							/* ignore whitespace at line start */
							continue;
						case '\n':
							/* ignore empty lines */
							i = 0;
							continue;
						case AVP_NAME_CHAR:
							state = IN_NAME;
							i = 0;
							name[i++] = c;
							name[i] = '\0';
							snprintf(linenum_buf,MAX_ITEM_LEN,"%s:%u",filename,linenum);
							curr = new_avpl(linenum_buf);
							continue;
						case '#':
							state = MY_IGNORE;
							continue;
						default:
							loal_error = load_loal_error(fp,loal,curr,linenum,"expecting name got: '%c'",c);
							goto error;
					}
				case BEFORE_NAME:
					i = 0;
					name[0] = '\0';
					switch (c) {
						case '\\':
							c = (char) fgetc(fp);
							if (c != '\n') ungetc(c,fp);
							continue;
						case ' ':
						case '\t':
							continue;
						case AVP_NAME_CHAR:
							state = IN_NAME;

							name[i++] = c;
							name[i] = '\0';
							continue;
						case '\n':
							loal_append(loal,curr);
							curr = NULL;
							state = START;
							continue;
						default:
							loal_error = load_loal_error(fp,loal,curr,linenum,"expecting name got: '%c'",c);
							goto error;
					}
					case IN_NAME:
						switch (c) {
							case ';':
								state = BEFORE_NAME;

								op = '?';
								name[i] = '\0';
								value[0] = '\0';
								i = 0;

								avp = new_avp(name,value,op);

								if (! insert_avp(curr,avp) ) {
									delete_avp(avp);
								}

								continue;
							case AVP_OP_CHAR:
								name[i] = '\0';
								i = 0;
								op = c;
								state = IN_VALUE;
								continue;
							case AVP_NAME_CHAR:
								name[i++] = c;
								continue;
							case '\n':
								loal_error = load_loal_error(fp,loal,curr,linenum,"operator expected found new line");
								goto error;
							default:
								loal_error = load_loal_error(fp,loal,curr,linenum,"name or match operator expected found '%c'",c);
								goto error;
						}
					case IN_VALUE:
						switch (c) {
							case '\\':
								value[i++] = (char) fgetc(fp);
								continue;
							case ';':
								state = BEFORE_NAME;

								value[i] = '\0';
								i = 0;

								avp = new_avp(name,value,op);

								if (! insert_avp(curr,avp) ) {
									delete_avp(avp);
								}
								continue;
							case '\n':
								loal_error = load_loal_error(fp,loal,curr,linenum,"';' expected found new line");
								goto error;
							default:
								value[i++] = c;
								continue;
						}
			}
		}
		fclose (fp);

		if (curr) {
			// Premature EOF? It could just be a file that doesn't
			// end in a newline, but hard to say without checking
			// state. Error, discard, add to existing loal?
			delete_avpl(curr,true);
		}

		g_free(linenum_buf);
		g_free(name);
		g_free(value);

		return loal;

	} else {
		report_open_failure(filename,errno,false);
		loal_error = load_loal_error(NULL,loal,NULL,0,"Cannot Open file '%s'",filename);
	}

error:
	g_free(linenum_buf);
	g_free(name);
	g_free(value);

	return loal_error;
}
