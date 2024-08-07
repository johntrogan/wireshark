/* packet-xmpp-utils.c
 * Wireshark's XMPP dissector.
 *
 * Copyright 2011, Mariusz Okroj <okrojmariusz[]gmail.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/strutil.h>
#include <epan/exceptions.h>

#include "packet-xmpp.h"
#include "packet-xmpp-core.h"
#include "packet-xmpp-utils.h"

static void
xmpp_copy_hash_table_func(void *key, void *value, void *user_data)
{
    GHashTable *dst = (GHashTable *)user_data;
    g_hash_table_insert(dst, key, value);
}

static void xmpp_copy_hash_table(GHashTable *src, GHashTable *dst)
{
    g_hash_table_foreach(src, xmpp_copy_hash_table_func, dst);
}

static GList* xmpp_find_element_by_name(xmpp_element_t *packet,const char *name);
static char* xmpp_ep_string_upcase(wmem_allocator_t *pool, const char* string);
static void xmpp_unknown_attrs(proto_tree *tree, tvbuff_t *tvb, packet_info *pinfo _U_, xmpp_element_t *element, bool displ_short_list);


void
xmpp_iq_reqresp_track(packet_info *pinfo, xmpp_element_t *packet, xmpp_conv_info_t *xmpp_info)
{
    xmpp_transaction_t *xmpp_trans = NULL;

    xmpp_attr_t *attr_id;
    char *id;

    attr_id = xmpp_get_attr(packet, "id");

    if (!attr_id) {
        return;
    }

    id = wmem_strdup(pinfo->pool, attr_id->value);

    if (!pinfo->fd->visited) {
        xmpp_trans = (xmpp_transaction_t *)wmem_tree_lookup_string(xmpp_info->req_resp, id, WMEM_TREE_STRING_NOCASE);
        if (xmpp_trans) {
            xmpp_trans->resp_frame = pinfo->num;

        } else {
            char *se_id = wmem_strdup(wmem_file_scope(), id);

            xmpp_trans = wmem_new(wmem_file_scope(), xmpp_transaction_t);
            xmpp_trans->req_frame = pinfo->num;
            xmpp_trans->resp_frame = 0;

            wmem_tree_insert_string(xmpp_info->req_resp, se_id, (void *) xmpp_trans, WMEM_TREE_STRING_NOCASE);

        }

    } else {
        wmem_tree_lookup_string(xmpp_info->req_resp, id, WMEM_TREE_STRING_NOCASE);
    }
}

void
xmpp_jingle_session_track(packet_info *pinfo, xmpp_element_t *packet, xmpp_conv_info_t *xmpp_info)
{
    xmpp_element_t *jingle_packet;
    GList *jingle_packet_l;

    jingle_packet_l = xmpp_find_element_by_name(packet,"jingle");
    jingle_packet = (xmpp_element_t *)(jingle_packet_l?jingle_packet_l->data:NULL);

    if (jingle_packet && !pinfo->fd->visited) {
        xmpp_attr_t *attr_id;
        xmpp_attr_t *attr_sid;

        char *se_id;
        char *se_sid;


        attr_id = xmpp_get_attr(packet, "id");
        if (!attr_id) {
            return;
        }

        attr_sid = xmpp_get_attr(jingle_packet, "sid");
        if (!attr_sid) {
            return;
        }

        se_id = wmem_strdup(wmem_file_scope(), attr_id->value);
        se_sid = wmem_strdup(wmem_file_scope(), attr_sid->value);

        wmem_tree_insert_string(xmpp_info->jingle_sessions, se_id, (void*) se_sid, WMEM_TREE_STRING_NOCASE);
    }
}

void
xmpp_gtalk_session_track(packet_info *pinfo, xmpp_element_t *packet, xmpp_conv_info_t *xmpp_info)
{
    xmpp_element_t *gtalk_packet;
    GList *gtalk_packet_l;

    gtalk_packet_l = xmpp_find_element_by_name(packet,"session");
    gtalk_packet = (xmpp_element_t *)(gtalk_packet_l?gtalk_packet_l->data:NULL);


    if (gtalk_packet && !pinfo->fd->visited) {
        xmpp_attr_t *attr_id;
        xmpp_attr_t *attr_sid;

        char *se_id;
        char *se_sid;

        xmpp_attr_t *xmlns = xmpp_get_attr(gtalk_packet, "xmlns");
        if(xmlns && strcmp(xmlns->value,"http://www.google.com/session") != 0)
            return;

        attr_id = xmpp_get_attr(packet, "id");
        if (!attr_id) {
            return;
        }

        attr_sid = xmpp_get_attr(gtalk_packet, "id");
        if (!attr_sid) {
            return;
        }

        se_id = wmem_strdup(wmem_file_scope(), attr_id->value);
        se_sid = wmem_strdup(wmem_file_scope(), attr_sid->value);

        wmem_tree_insert_string(xmpp_info->gtalk_sessions, se_id, (void*) se_sid, WMEM_TREE_STRING_NOCASE);
    }
}

void
xmpp_ibb_session_track(packet_info *pinfo, xmpp_element_t *packet, xmpp_conv_info_t *xmpp_info)
{
    xmpp_element_t *ibb_packet = NULL;
    GList *ibb_packet_l;

    if(strcmp(packet->name, "message") == 0)
    {
        ibb_packet_l = xmpp_find_element_by_name(packet,"data");
        ibb_packet = (xmpp_element_t *)(ibb_packet_l?ibb_packet_l->data:NULL);

    } else if(strcmp(packet->name, "iq") == 0)
    {
        ibb_packet_l = xmpp_find_element_by_name(packet,"open");

        if(!ibb_packet_l)
            ibb_packet_l = xmpp_find_element_by_name(packet,"close");
         if(!ibb_packet_l)
            ibb_packet_l = xmpp_find_element_by_name(packet,"data");

        ibb_packet = (xmpp_element_t *)(ibb_packet_l?ibb_packet_l->data:NULL);
    }

    if (ibb_packet && !pinfo->fd->visited) {
        xmpp_attr_t *attr_id;
        xmpp_attr_t *attr_sid;

        char *se_id;
        char *se_sid;


        attr_id = xmpp_get_attr(packet, "id");
        attr_sid = xmpp_get_attr(ibb_packet, "sid");
        if(attr_id && attr_sid)
        {
            se_id = wmem_strdup(wmem_file_scope(), attr_id->value);
            se_sid = wmem_strdup(wmem_file_scope(), attr_sid->value);
            wmem_tree_insert_string(xmpp_info->ibb_sessions, se_id, (void*) se_sid, WMEM_TREE_STRING_NOCASE);
        }
    }
}

static void
// NOLINTNEXTLINE(misc-no-recursion)
xmpp_unknown_items(proto_tree *tree, tvbuff_t *tvb, packet_info *pinfo, xmpp_element_t *element, unsigned level)
{
    GList *childs = element->elements;

    DISSECTOR_ASSERT( level < ETT_UNKNOWN_LEN );

    xmpp_unknown_attrs(tree, tvb, pinfo, element, true);

    if(element->data)
    {
        proto_tree_add_string(tree, hf_xmpp_cdata, tvb, element->data->offset, element->data->length, element->data->value);
    }

    while(childs)
    {
        xmpp_element_t *child = (xmpp_element_t *)childs->data;
        proto_item *child_item;
        proto_tree *child_tree = proto_tree_add_subtree(tree, tvb, child->offset, child->length,
            ett_unknown[level], &child_item, xmpp_ep_string_upcase(pinfo->pool, child->name));

        if(child->default_ns_abbrev)
            proto_item_append_text(child_item, "(%s)", child->default_ns_abbrev);

        xmpp_unknown_items(child_tree, tvb, pinfo, child, level +1);

        childs = childs->next;
    }
}

void
xmpp_unknown(proto_tree *tree, tvbuff_t *tvb, packet_info *pinfo, xmpp_element_t *element)
{
    GList *childs = element->elements;

    /*element has unrecognized elements*/
    while(childs)
    {
        xmpp_element_t *child = (xmpp_element_t *)childs->data;
        if(!child->was_read)
        {
            proto_item *unknown_item;
            proto_tree *unknown_tree;

            unknown_item = proto_tree_add_string_format(tree,
                    hf_xmpp_unknown, tvb, child->offset, child->length, child->name,
                    "%s", xmpp_ep_string_upcase(pinfo->pool, child->name));

            unknown_tree = proto_item_add_subtree(unknown_item, ett_unknown[0]);

            /*Add COL_INFO only if root element is IQ*/
            if(strcmp(element->name,"iq")==0)
                col_append_fstr(pinfo->cinfo, COL_INFO, "%s ", xmpp_ep_string_upcase(pinfo->pool, child->name));

            if(child->default_ns_abbrev)
                proto_item_append_text(unknown_item,"(%s)",child->default_ns_abbrev);

            xmpp_unknown_items(unknown_tree, tvb, pinfo, child, 1);
            proto_item_append_text(unknown_item, " [UNKNOWN]");
            expert_add_info_format(pinfo, unknown_item, &ei_xmpp_unknown_element, "Unknown element: %s", child->name);
        }
        childs = childs->next;
    }
}

static void
cleanup_glist_cb(void *user_data) {
    GList *li = (GList*)user_data;

    g_list_free(li);
}

static void
xmpp_unknown_attrs(proto_tree *tree, tvbuff_t *tvb, packet_info *pinfo _U_, xmpp_element_t *element, bool displ_short_list)
{
    proto_item *item = proto_tree_get_parent(tree);

    GList *keys = g_hash_table_get_keys(element->attrs);
    GList *values = g_hash_table_get_values(element->attrs);

    GList *keys_head = keys, *values_head = values;

    CLEANUP_PUSH_PFX(k, cleanup_glist_cb, keys_head);
    CLEANUP_PUSH_PFX(v, cleanup_glist_cb, values_head);

    bool short_list_started = false;

    while(keys && values)
    {
        xmpp_attr_t *attr = (xmpp_attr_t*) values->data;
        if (!attr->was_read) {
            if (displ_short_list) {
                if (!short_list_started)
                    proto_item_append_text(item, " [");
                else
                    proto_item_append_text(item, " ");
                proto_item_append_text(item, "%s=\"%s\"", (char*) keys->data, attr->value);

                short_list_started = true;
            }

            /*If unknown element has xmlns attrib then header field hf_xmpp_xmlns is added to the tree.
             In other case only text.*/
            if (strcmp((const char *)keys->data, "xmlns") == 0)
                proto_tree_add_string(tree, hf_xmpp_xmlns, tvb, attr->offset, attr->length, attr->value);
            else {
                /*xmlns may looks like xmlns:abbrev="sth"*/
                const char *xmlns_needle = ws_ascii_strcasestr((const char *)keys->data, "xmlns:");
                if (xmlns_needle && xmlns_needle == keys->data) {
                    proto_tree_add_string_format(tree, hf_xmpp_xmlns, tvb, attr->offset, attr->length, attr->value,"%s: %s", (char*)keys->data, attr->value);
                } else {
                    proto_item* unknown_attr_item;
                    unknown_attr_item = proto_tree_add_string_format(tree,
                            hf_xmpp_unknown_attr, tvb, attr->offset, attr->length,
                            attr->name, "%s: %s", attr->name, attr->value);
                    proto_item_append_text(unknown_attr_item, " [UNKNOWN ATTR]");
                    expert_add_info_format(pinfo, unknown_attr_item, &ei_xmpp_unknown_attribute, "Unknown attribute %s", attr->name);
                }
            }
        }
        keys = keys->next;
        values = values->next;
    }

    if(short_list_started && displ_short_list)
        proto_item_append_text(item, "]");

    CLEANUP_CALL_AND_POP_PFX(v);
    CLEANUP_CALL_AND_POP_PFX(k);
}

void
xmpp_cdata(proto_tree *tree, tvbuff_t *tvb, xmpp_element_t *element, int hf)
{
    if(element->data)
{
        if (hf == -1) {
            proto_tree_add_string(tree, hf_xmpp_cdata, tvb, element->data->offset, element->data->length, element->data->value);
        } else {
            proto_tree_add_string(tree, hf, tvb, element->data->offset, element->data->length, element->data->value);
        }
    } else
    {
        if (hf == -1) {
            proto_tree_add_string_format_value(tree, hf_xmpp_cdata, tvb, 0, 0, "", "(empty)");
        } else {
            proto_tree_add_string(tree, hf, tvb, 0, 0, "");
        }
    }
}

/* displays element that looks like <element_name>element_value</element_name>
 * ELEMENT_NAME: element_value as TEXT in PROTO_TREE
 */
void
xmpp_simple_cdata_elem(proto_tree *tree, tvbuff_t *tvb, packet_info *pinfo _U_, xmpp_element_t *element)
{
    proto_tree_add_string_format(tree, hf_xmpp_cdata, tvb, element->offset, element->length, xmpp_elem_cdata(element),
                                    "%s: %s", xmpp_ep_string_upcase(pinfo->pool, element->name), xmpp_elem_cdata(element));
}

xmpp_array_t*
xmpp_ep_init_array_t(wmem_allocator_t *pool, const char** array, int len)
{
    xmpp_array_t *result;

    result = wmem_new(pool, xmpp_array_t);
    result->data = (void *) array;
    result->length = len;

    return result;
}

xmpp_attr_t*
xmpp_ep_init_attr_t(wmem_allocator_t *pool, const char *value, int offset, int length)
{
    xmpp_attr_t *result;
    result = wmem_new(pool, xmpp_attr_t);
    result->value = value;
    result->offset = offset;
    result->length = length;
    result->name = NULL;

    return result;
}

static char*
xmpp_ep_string_upcase(wmem_allocator_t *pool, const char* string)
{
    int len = (int)strlen(string);
    int i;
    char* result = (char *)wmem_alloc0(pool, len+1);
    for(i=0; i<len; i++)
    {
        result[i] = string[i];

        if(string[i]>='a' && string[i]<='z')
            result[i]-='a'-'A';

    }
    return result;
}

static int
xmpp_element_t_cmp(const void *a, const void *b)
{
    int result = strcmp(((const xmpp_element_t*)a)->name,((const xmpp_element_t*)b)->name);

    if(result == 0 && ((const xmpp_element_t*)a)->was_read)
        result = -1;

    return result;
}

static GList*
xmpp_find_element_by_name(xmpp_element_t *packet,const char *name)
{
    GList *found_elements;
    xmpp_element_t *search_element;

    /*create fake element only with name*/
    search_element = wmem_new(wmem_packet_scope(), xmpp_element_t);
    search_element->name = wmem_strdup(wmem_packet_scope(), name);

    found_elements = g_list_find_custom(packet->elements, search_element, xmpp_element_t_cmp);

    if(found_elements)
        return found_elements;
    else
        return NULL;
}


/* steal_*
 * function searches element in packet and sets it as read.
 * if element doesn't exist, NULL is returned.
 * If element is set as read, it is invisible for these functions.*/
xmpp_element_t*
xmpp_steal_element_by_name(xmpp_element_t *packet,const char *name)
{
    GList *element_l;
    xmpp_element_t *element = NULL;

    element_l = xmpp_find_element_by_name(packet, name);

    if(element_l)
    {
        element = (xmpp_element_t *)element_l->data;
        element->was_read = true;
    }

    return element;

}

xmpp_element_t*
xmpp_steal_element_by_names(xmpp_element_t *packet, const char **names, int names_len)
{
    int i;
    xmpp_element_t *el = NULL;

    for(i = 0; i<names_len; i++)
    {
        if((el = xmpp_steal_element_by_name(packet, names[i])))
            break;
    }

    return el;
}

xmpp_element_t*
xmpp_steal_element_by_attr(xmpp_element_t *packet, const char *attr_name, const char *attr_value)
{
    GList *childs = packet->elements;
    xmpp_element_t *result = NULL;

    while (childs) {
        xmpp_element_t *child_elem = (xmpp_element_t *)childs->data;
        xmpp_attr_t *attr = xmpp_get_attr(child_elem, attr_name);

        if(attr)
            attr->was_read = false;

        if (!child_elem->was_read && attr && strcmp(attr->value, attr_value) == 0) {

            result = (xmpp_element_t *)childs->data;

            result->was_read = true;

            break;
        } else
            childs = childs->next;
    }

    return result;
}

xmpp_element_t*
xmpp_steal_element_by_name_and_attr(xmpp_element_t *packet, const char *name, const char *attr_name, const char *attr_value)
{
    GList *childs = packet->elements;
    xmpp_element_t *result = NULL;

    while (childs) {
        xmpp_element_t *child_elem = (xmpp_element_t *)childs->data;
        xmpp_attr_t *attr = xmpp_get_attr(child_elem, attr_name);

        if(attr)
            attr->was_read = false;

        if (!child_elem->was_read && attr && strcmp(child_elem->name, name) == 0 && strcmp(attr->value, attr_value) == 0) {

            result = (xmpp_element_t *)childs->data;

            result->was_read = true;

            break;
        } else
            childs = childs->next;
    }
    return result;
}

xmpp_element_t*
xmpp_get_first_element(xmpp_element_t *packet)
{
    if(packet->elements && packet->elements->data)
        return (xmpp_element_t *)packet->elements->data;
    else
        return NULL;
}

static void
xmpp_element_t_cleanup(void* userdata)
{
    xmpp_element_t *node = (xmpp_element_t*)userdata;

    xmpp_element_t_tree_free(node);
}

/*
Function converts xml_frame_t structure to xmpp_element_t (simpler representation)
*/
xmpp_element_t*
// NOLINTNEXTLINE(misc-no-recursion)
xmpp_xml_frame_to_element_t(packet_info *pinfo, xml_frame_t *xml_frame, xmpp_element_t *parent, tvbuff_t *tvb)
{
    xml_frame_t *child;
    xmpp_element_t *node = wmem_new0(pinfo->pool, xmpp_element_t);

    tvbparse_t* tt;
    tvbparse_elem_t* elem;

    node->attrs = g_hash_table_new(g_str_hash, g_str_equal);
    node->elements = NULL;
    node->data = NULL;
    node->was_read = false;
    node->default_ns_abbrev = NULL;

    node->name = wmem_strdup(pinfo->pool, xml_frame->name_orig_case);
    node->offset = 0;
    node->length = 0;

    node->namespaces = g_hash_table_new(g_str_hash, g_str_equal);
    if(parent)
    {
        xmpp_copy_hash_table(parent->namespaces, node->namespaces);
    } else
    {
        g_hash_table_insert(node->namespaces, (void *)"", (void *)"jabber:client");
    }

    node->offset = xml_frame->start_offset;
    node->length = xml_frame->length;

    /* We might throw an exception recursively creating child nodes.
     * Make sure we free the GHashTables created above (and the GList
     * or child nodes already added) if that happens.
     */
    CLEANUP_PUSH(xmpp_element_t_cleanup, node);

    tt = tvbparse_init(pinfo->pool, tvb,node->offset,-1,NULL,want_ignore);

    if((elem = tvbparse_get(tt,want_stream_end_with_ns))!=NULL)
    {
        node->default_ns_abbrev = tvb_get_string_enc(pinfo->pool, elem->sub->tvb, elem->sub->offset, elem->sub->len, ENC_ASCII);
    }

    child = xml_frame->first_child;

    while(child)
    {
        if(child->type != XML_FRAME_TAG)
        {
            if(child->type == XML_FRAME_ATTRIB)
            {
                int l;
                char *value = NULL;
                const char *xmlns_needle = NULL;

                xmpp_attr_t *attr = wmem_new(pinfo->pool, xmpp_attr_t);
                attr->length = 0;
                attr->offset = 0;
                attr->was_read = false;

                if (child->value != NULL) {
                    l = tvb_reported_length(child->value);
                    value = (char *)wmem_alloc0(pinfo->pool, l + 1);
                    tvb_memcpy(child->value, value, 0, l);
                }

                attr->offset = child->start_offset;
                attr->length = child->length;
                attr->value = value;
                attr->name = wmem_strdup(pinfo->pool, child->name_orig_case);

                g_hash_table_insert(node->attrs,(void *)attr->name,(void *)attr);

                /*checking that attr->name looks like xmlns:ns*/
                xmlns_needle = ws_ascii_strcasestr(attr->name, "xmlns");

                if(xmlns_needle == attr->name)
                {
                    if(attr->name[5] == ':' && strlen(attr->name) > 6)
                    {
                        g_hash_table_insert(node->namespaces, (void *)wmem_strdup(pinfo->pool, &attr->name[6]), (void *)wmem_strdup(pinfo->pool, attr->value));
                    } else if(attr->name[5] == '\0')
                    {
                        g_hash_table_insert(node->namespaces, (void *)"", (void *)wmem_strdup(pinfo->pool, attr->value));
                    }
                }


            }
            else if( child->type == XML_FRAME_CDATA)
            {
                xmpp_data_t *data = NULL;
                int l;
                char* value = NULL;

                data =  wmem_new(pinfo->pool, xmpp_data_t);
                data->length = 0;
                data->offset = 0;

                if (child->value != NULL) {
                    l = tvb_reported_length(child->value);
                    value = (char *)wmem_alloc0(pinfo->pool, l + 1);
                    tvb_memcpy(child->value, value, 0, l);
                }

                data->value = value;

                data->offset = child->start_offset;
                data->length = child->length;
                node->data = data;
            }
        } else
        {
            increment_dissection_depth(pinfo);
            node->elements = g_list_append(node->elements,(void *)xmpp_xml_frame_to_element_t(pinfo, child, node,tvb));
            decrement_dissection_depth(pinfo);
        }

        child = child->next_sibling;
    }

    CLEANUP_POP;

    return node;
}

void
// NOLINTNEXTLINE(misc-no-recursion)
xmpp_element_t_tree_free(xmpp_element_t *root)
{
    GList *childs = root->elements;

    g_hash_table_destroy(root->attrs);
    g_hash_table_destroy(root->namespaces);

    while(childs)
    {
        xmpp_element_t *child = (xmpp_element_t *)childs->data;

        // Our depth should be limited by the check in xmpp_xml_frame_to_element_t
        xmpp_element_t_tree_free(child);
        childs = childs->next;
    }
    g_list_free(root->elements);
}

/*Function recognize attribute names if they looks like xmlns:ns*/
static gboolean
attr_find_pred(void *key, void *value _U_, void *user_data)
{
    char *attr_name = (char*) user_data;

    if( strcmp(attr_name, "xmlns") == 0 )
    {
        const char *first_occur = ws_ascii_strcasestr((const char *)key, "xmlns:");
        if(first_occur && first_occur == key)
            return true;
        else
            return false;
    }
    return false;
}

/*Functions returns element's attribute by name and set as read*/
xmpp_attr_t*
xmpp_get_attr(xmpp_element_t *element, const char* attr_name)
{
    xmpp_attr_t *result = (xmpp_attr_t *)g_hash_table_lookup(element->attrs, attr_name);

    if(!result)
    {
        result = (xmpp_attr_t *)g_hash_table_find(element->attrs, attr_find_pred, (void *)attr_name);
    }

    if(result)
        result->was_read = true;

    return result;
}

/*Functions returns element's attribute by name and namespace abbrev*/
static xmpp_attr_t*
xmpp_get_attr_ext(packet_info *pinfo, xmpp_element_t *element, const char* attr_name, const char* ns_abbrev)
{
    char* search_phrase;
    xmpp_attr_t *result;

    if(strcmp(ns_abbrev,"")==0)
        search_phrase = wmem_strdup(pinfo->pool, attr_name);
    else if(strcmp(attr_name, "xmlns") == 0)
        search_phrase = wmem_strdup_printf(pinfo->pool, "%s:%s",attr_name, ns_abbrev);
    else
        search_phrase = wmem_strdup_printf(pinfo->pool, "%s:%s", ns_abbrev, attr_name);

    result = (xmpp_attr_t *)g_hash_table_lookup(element->attrs, search_phrase);

    if(!result)
    {
        result = (xmpp_attr_t *)g_hash_table_find(element->attrs, attr_find_pred, (void *)attr_name);
    }

    if(result)
        result->was_read = true;

    return result;
}



char*
xmpp_element_to_string(wmem_allocator_t *pool, tvbuff_t *tvb, xmpp_element_t *element)
{
    char *buff = NULL;

    if(tvb_offset_exists(tvb, element->offset+element->length-1))
    {
        buff = tvb_get_string_enc(pool, tvb, element->offset, element->length, ENC_ASCII);
    }
    return buff;
}

static void
children_foreach_hide_func(proto_node *node, void *data)
{
    int *i = (int *)data;
    if((*i) == 0)
        proto_item_set_hidden(node);
    (*i)++;
}

static void
children_foreach_show_func(proto_node *node, void *data)
{
    int *i = (int *)data;
    if((*i) == 0)
        proto_item_set_visible(node);
    (*i)++;
}

void
xmpp_proto_tree_hide_first_child(proto_tree *tree)
{
    int i = 0;
    proto_tree_children_foreach(tree, children_foreach_hide_func, &i);
}

void
xmpp_proto_tree_show_first_child(proto_tree *tree)
{
    int i = 0;
    proto_tree_children_foreach(tree, children_foreach_show_func, &i);
}

char*
proto_item_get_text(wmem_allocator_t *pool, proto_item *item)
{
    field_info *fi = NULL;
    char *result;

    if(item == NULL)
        return NULL;

    fi = PITEM_FINFO(item);

    if(fi==NULL)
        return NULL;

    if (fi->rep == NULL)
        return NULL;


    result = wmem_strdup(pool, fi->rep->representation);
    return result;
}


void
xmpp_display_attrs(proto_tree *tree, xmpp_element_t *element, packet_info *pinfo, tvbuff_t *tvb, const xmpp_attr_info *attrs, unsigned n)
{
    proto_item *item = proto_tree_get_parent(tree);
    xmpp_attr_t *attr;
    unsigned i;
    bool short_list_started = false;

    if(element->default_ns_abbrev)
        proto_item_append_text(item, "(%s)",element->default_ns_abbrev);

    proto_item_append_text(item," [");
    for(i = 0; i < n && attrs!=NULL; i++)
    {
        attr = xmpp_get_attr(element, attrs[i].name);
        if(attr)
        {
            if(attrs[i].phf != NULL)
            {
                if(attr->name)
                    proto_tree_add_string_format(tree, *attrs[i].phf, tvb, attr->offset, attr->length, attr->value,"%s: %s", attr->name, attr->value);
                else
                    proto_tree_add_string(tree, *attrs[i].phf, tvb, attr->offset, attr->length, attr->value);
            }
            else
            {
                proto_tree_add_string_format(tree, hf_xmpp_attribute, tvb, attr->offset, attr->length, attr->value,
                    "%s: %s", attr->name?attr->name:attrs[i].name, attr->value);
            }

            if(attrs[i].in_short_list)
            {
                if(short_list_started)
                {
                    proto_item_append_text(item," ");
                }
                proto_item_append_text(item,"%s=\"%s\"",attr->name?attr->name:attrs[i].name, attr->value);
                short_list_started = true;
            }

        } else if(attrs[i].is_required)
        {
            expert_add_info_format(pinfo, item, &ei_xmpp_required_attribute, "Required attribute \"%s\" doesn't appear in \"%s\".", attrs[i].name, element->name);
        }

        if(attrs[i].val_func)
        {
            if(attr)
                attrs[i].val_func(pinfo, item, attrs[i].name, attr->value, attrs[i].data);
            else
                attrs[i].val_func(pinfo, item, attrs[i].name, NULL, attrs[i].data);
        }
    }
    proto_item_append_text(item,"]");

    /*displays attributes that weren't recognized*/
    xmpp_unknown_attrs(tree, tvb, pinfo, element, false);
}

void
xmpp_display_attrs_ext(proto_tree *tree, xmpp_element_t *element, packet_info *pinfo, tvbuff_t *tvb, const xmpp_attr_info_ext *attrs, unsigned n)
{
    proto_item *item = proto_tree_get_parent(tree);
    xmpp_attr_t *attr;
    unsigned i;
    bool short_list_started = false;

    GList *ns_abbrevs_head, *ns_abbrevs = g_hash_table_get_keys(element->namespaces);
    GList *ns_fullnames_head, *ns_fullnames = g_hash_table_get_values(element->namespaces);
    ns_abbrevs_head = ns_abbrevs;
    ns_fullnames_head = ns_fullnames;

    if(element->default_ns_abbrev)
        proto_item_append_text(item, "(%s)",element->default_ns_abbrev);

    proto_item_append_text(item," [");
    while(ns_abbrevs && ns_fullnames)
    {
        for (i = 0; i < n && attrs != NULL; i++) {
            if(strcmp((const char *)(ns_fullnames->data), attrs[i].ns) == 0)
            {
                attr = xmpp_get_attr_ext(pinfo, element, attrs[i].info.name, (const char *)(ns_abbrevs->data));
                if(!attr && element->default_ns_abbrev && strcmp((const char *)ns_abbrevs->data, element->default_ns_abbrev)==0)
                    attr = xmpp_get_attr_ext(pinfo, element, attrs[i].info.name, "");

                if (attr) {
                    if (attrs[i].info.phf != NULL) {
                        if (attr->name)
                            proto_tree_add_string_format(tree, *attrs[i].info.phf, tvb, attr->offset, attr->length, attr->value, "%s: %s", attr->name, attr->value);
                        else
                            proto_tree_add_string(tree, *attrs[i].info.phf, tvb, attr->offset, attr->length, attr->value);
                    } else {
                        proto_tree_add_string_format(tree, hf_xmpp_attribute, tvb, attr->offset, attr->length, attr->value,
                            "%s: %s", attr->name ? attr->name : attrs[i].info.name, attr->value);
                    }

                    if (attrs[i].info.in_short_list) {
                        if (short_list_started) {
                            proto_item_append_text(item, " ");
                        }
                        proto_item_append_text(item, "%s=\"%s\"", attr->name ? attr->name : attrs[i].info.name, attr->value);
                        short_list_started = true;
                    }

                } else if (attrs[i].info.is_required) {
                    expert_add_info_format(pinfo, item, &ei_xmpp_required_attribute, "Required attribute \"%s\" doesn't appear in \"%s\".", attrs[i].info.name, element->name);
                }

                if (attrs[i].info.val_func) {
                    if (attr)
                        attrs[i].info.val_func(pinfo, item, attrs[i].info.name, attr->value, attrs[i].info.data);
                    else
                        attrs[i].info.val_func(pinfo, item, attrs[i].info.name, NULL, attrs[i].info.data);
                }
            }
        }
        ns_abbrevs = ns_abbrevs->next;
        ns_fullnames = ns_fullnames->next;
    }
    proto_item_append_text(item,"]");

    /*displays attributes that weren't recognized*/
    xmpp_unknown_attrs(tree, tvb, pinfo, element, false);

    g_list_free(ns_abbrevs_head);
    g_list_free(ns_fullnames_head);
}

typedef struct _name_attr_t
{
    const char *name;
    const char *attr_name;
    const char *attr_value;
} name_attr_t;

/*
returns pointer to the struct that contains 3 strings(element name, attribute name, attribute value)
*/
void *
xmpp_name_attr_struct(wmem_allocator_t *pool, const char *name, const char *attr_name, const char *attr_value)
{
    name_attr_t *result;

    result =  wmem_new(pool, name_attr_t);
    result->name = name;
    result->attr_name = attr_name;
    result->attr_value = attr_value;
    return result;
}

void
xmpp_display_elems(proto_tree *tree, xmpp_element_t *parent, packet_info *pinfo, tvbuff_t *tvb, xmpp_elem_info *elems, unsigned n)
{
    unsigned i;

    for(i = 0; i < n && elems!=NULL; i++)
    {
        xmpp_element_t *elem = NULL;

        if(elems[i].type == NAME_AND_ATTR)
        {
            bool loop = true;

            const name_attr_t *a = (const name_attr_t *)(elems[i].data);

            while(loop && (elem = xmpp_steal_element_by_name_and_attr(parent, a->name, a->attr_name, a->attr_value))!=NULL)
            {
                elems[i].elem_func(tree, tvb, pinfo, elem);
                if(elems[i].occurrence == ONE)
                    loop = false;
            }
        } else if(elems[i].type == NAME)
        {
            bool loop = true;
            const char *name = (const char *)(elems[i].data);

            while(loop && (elem = xmpp_steal_element_by_name(parent, name))!=NULL)
            {
                elems[i].elem_func(tree, tvb, pinfo, elem);
                if(elems[i].occurrence == ONE)
                    loop = false;
            }
        }
        else if(elems[i].type == ATTR)
        {
            bool loop = true;
            const name_attr_t *attr = (const name_attr_t *)(elems[i].data);

            while(loop && (elem = xmpp_steal_element_by_attr(parent, attr->attr_name, attr->attr_value))!=NULL)
            {
                elems[i].elem_func(tree, tvb, pinfo, elem);
                if(elems[i].occurrence == ONE)
                    loop = false;
            }

        } else if(elems[i].type == NAMES)
        {
            bool loop = true;
            const xmpp_array_t *names = (const xmpp_array_t *)(elems[i].data);

            while(loop && (elem =  xmpp_steal_element_by_names(parent, (const char**)names->data, names->length))!=NULL)
            {
                elems[i].elem_func(tree, tvb, pinfo, elem);
                if(elems[i].occurrence == ONE)
                    loop = false;
            }
        }
    }

    xmpp_unknown(tree, tvb, pinfo, parent);
}

/*
function checks that variable value is in array ((xmpp_array_t)data)->data
*/
void
xmpp_val_enum_list(packet_info *pinfo, proto_item *item, const char *name, const char *value, const void *data)
{
    const xmpp_array_t *enums_array = (const xmpp_array_t *)data;

    int i;
    bool value_in_enums = false;

    char **enums =  (char**)enums_array->data;

    if (value != NULL) {
        for (i = 0; i < enums_array->length; i++) {
            if (strcmp(value, enums[i]) == 0) {
                value_in_enums = true;
                break;
            }
        }
        if (!value_in_enums) {
            expert_add_info_format(pinfo, item, &ei_xmpp_field_unexpected_value, "Field \"%s\" has unexpected value \"%s\"", name, value);
        }
    }
}


void
xmpp_change_elem_to_attrib(wmem_allocator_t *pool, const char *elem_name, const char *attr_name, xmpp_element_t *parent, xmpp_attr_t* (*transform_func)(wmem_allocator_t *pool, xmpp_element_t *element))
{
    xmpp_element_t *element = NULL;
    xmpp_attr_t *fake_attr = NULL;

    element = xmpp_steal_element_by_name(parent, elem_name);
    if(element)
        fake_attr = transform_func(pool, element);

    if(fake_attr)
        g_hash_table_insert(parent->attrs, (void *)attr_name, fake_attr);
}

xmpp_attr_t*
xmpp_transform_func_cdata(wmem_allocator_t *pool, xmpp_element_t *elem)
{
    xmpp_attr_t *result = xmpp_ep_init_attr_t(pool, elem->data?elem->data->value:"", elem->offset, elem->length);
    return result;
}

#if 0
static void
printf_hash_table_func(void *key, void *value, void *user_data _U_)
{
    printf("'%s' '%s'\n", (char*)key, (char*)value);
}

void
printf_elements(xmpp_element_t *root)
{
    GList *elems = root->elements;

    printf("%s\n", root->name);
    g_hash_table_foreach(root->namespaces, printf_hash_table_func, NULL);
    while(elems)
    {
        printf_elements(elems->data);
        elems = elems->next;
    }
}
#endif

/*
* Editor modelines - https://www.wireshark.org/tools/modelines.html
*
* Local variables:
* c-basic-offset: 4
* tab-width: 8
* indent-tabs-mode: nil
* End:
*
* ex: set shiftwidth=4 tabstop=8 expandtab:
* :indentSize=4:tabSize=8:noTabs=true:
*/
