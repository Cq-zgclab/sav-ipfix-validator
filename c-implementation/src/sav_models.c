/**
 * @file sav_models.c
 * @brief Fixed SAV rule universe + exporter-side observation models (T1/T2/T3)
 */

#include "sav_models.h"

#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <glib.h>

#include "sav_ie_definitions.h"

/* ---- Constants (fixed universe) ---- */

#define SAV_T3_IPV4_PREFIXLEN 24
#define SAV_T3_IPV6_PREFIXLEN 48

/* libfixbuf semantic values (observed convention)
 * - allOf: 3
 * - exactlyOneOf: 1
 */
#define SAV_STL_SEM_ALL_OF        3
#define SAV_STL_SEM_EXACTLY_ONEOF 1

/* ---- Utilities ---- */

static void ipv4_prefix_mask(uint8_t out_prefix[4], const uint8_t in_ip[4], uint8_t prefix_len)
{
    uint32_t ip;
    memcpy(&ip, in_ip, 4);
    ip = ntohl(ip);

    uint32_t mask = (prefix_len == 0) ? 0 : (0xFFFFFFFFu << (32 - prefix_len));
    uint32_t pref = ip & mask;
    pref = htonl(pref);
    memcpy(out_prefix, &pref, 4);
}

static void ipv6_prefix_mask(uint8_t out_prefix[16], const uint8_t in_ip[16], uint8_t prefix_len)
{
    memset(out_prefix, 0, 16);

    uint32_t full_bytes = prefix_len / 8;
    uint32_t rem_bits = prefix_len % 8;

    if (full_bytes > 16) full_bytes = 16;

    memcpy(out_prefix, in_ip, full_bytes);
    if (full_bytes < 16 && rem_bits != 0) {
        uint8_t mask = (uint8_t)(0xFFu << (8 - rem_bits));
        out_prefix[full_bytes] = (uint8_t)(in_ip[full_bytes] & mask);
    }
}

static gboolean env_enable_template_b(void)
{
    const char *v = getenv("SAV_ENABLE_TEMPLATE_B");
    return (v && v[0] != '\0' && strcmp(v, "0") != 0);
}

static gboolean env_enable_legacy_t123(void)
{
    const char *v = getenv("SAV_EXPORT_T123");
    return (v && v[0] != '\0' && strcmp(v, "0") != 0);
}

static gboolean env_enable_template_a_crossproduct(void)
{
    const char *v = getenv("SAV_DEMO_TEMPLATE_A_CROSSPRODUCT");
    return (v && v[0] != '\0' && strcmp(v, "0") != 0);
}

/* NOTE: For historical/libfixbuf reasons in this project, IPv4 prefixes inside STL entries
 * (templates 900/902) are stored as a host-order uint32 in memory (see generator helpers).
 * The packet source IP bytes are stored in network order.
 */
static gboolean ipv4_in_prefix_hostprefix(const uint8_t ip_net[4], const uint8_t prefix_host_bytes[4], uint8_t plen)
{
    if (plen == 0) return TRUE;
    uint32_t ip_u_net = 0;
    uint32_t p_u_host = 0;
    memcpy(&ip_u_net, ip_net, 4);
    memcpy(&p_u_host, prefix_host_bytes, 4);
    const uint32_t ip_u_host = ntohl(ip_u_net);
    const uint32_t mask = (plen >= 32) ? 0xFFFFFFFFu : (0xFFFFFFFFu << (32 - plen));
    return ((ip_u_host & mask) == (p_u_host & mask));
}

static void ipv4_prefix_mask_host(uint8_t out_prefix_host_bytes[4], const uint8_t in_prefix_host_bytes[4], uint8_t plen)
{
    uint32_t p = 0;
    memcpy(&p, in_prefix_host_bytes, 4);
    const uint32_t mask = (plen == 0) ? 0u : ((plen >= 32) ? 0xFFFFFFFFu : (0xFFFFFFFFu << (32 - plen)));
    p &= mask;
    memcpy(out_prefix_host_bytes, &p, 4);
}

static void ipv4_prefix_from_ip_net_host(uint8_t out_prefix_host_bytes[4], const uint8_t ip_net[4], uint8_t plen)
{
    uint32_t ip_u_net = 0;
    memcpy(&ip_u_net, ip_net, 4);
    uint32_t ip_u_host = ntohl(ip_u_net);
    const uint32_t mask = (plen == 0) ? 0u : ((plen >= 32) ? 0xFFFFFFFFu : (0xFFFFFFFFu << (32 - plen)));
    ip_u_host &= mask;
    memcpy(out_prefix_host_bytes, &ip_u_host, 4);
}

static gboolean ipv6_in_prefix(const uint8_t ip[16], const uint8_t prefix[16], uint8_t plen)
{
    const uint32_t full = plen / 8;
    const uint32_t rem = plen % 8;

    if (plen == 0) return TRUE;
    if (full > 16) return TRUE;

    if (full > 0 && memcmp(ip, prefix, full) != 0) return FALSE;
    if (rem == 0) return TRUE;
    if (full >= 16) return TRUE;
    const uint8_t mask = (uint8_t)(0xFFu << (8 - rem));
    return ((ip[full] & mask) == (prefix[full] & mask));
}

static void v4_make_alt_prefix_not_covering(
    uint8_t out_prefix[4],
    const uint8_t base_prefix[4],
    uint8_t plen,
    const uint8_t avoid_ip[4],
    const uint8_t *avoid_prefixes,
    size_t avoid_count,
    uint8_t salt)
{
    memcpy(out_prefix, base_prefix, 4);

    /* Deterministically perturb within the prefix portion to get a visibly different network.
     * Then re-mask to a canonical network address.
     */
    for (uint8_t tries = 0; tries < 64; tries++) {
        uint8_t cand[4];
        memcpy(cand, base_prefix, 4);

        /* Choose a byte inside the prefix region when possible; fall back to first byte. */
        uint8_t byte = (plen >= 16) ? 1 : 0;
        cand[byte] = (uint8_t)(cand[byte] ^ (uint8_t)(0x5Au + salt + tries));

        ipv4_prefix_mask_host(out_prefix, cand, plen);

        /* Must not cover the spoofed packet source IP. */
        if (ipv4_in_prefix_hostprefix(avoid_ip, out_prefix, plen)) {
            continue;
        }

        /* Must be distinct from previous prefixes (byte-wise). */
        gboolean dup = FALSE;
        for (size_t i = 0; i < avoid_count; i++) {
            const uint8_t *ap = avoid_prefixes + i * 4;
            if (memcmp(ap, out_prefix, 4) == 0) {
                dup = TRUE;
                break;
            }
        }
        if (dup) continue;
        return;
    }
}

static void v6_make_alt_prefix_not_covering(
    uint8_t out_prefix[16],
    const uint8_t base_prefix[16],
    uint8_t plen,
    const uint8_t avoid_ip[16],
    const uint8_t *avoid_prefixes,
    size_t avoid_count,
    uint8_t salt)
{
    memcpy(out_prefix, base_prefix, 16);

    for (uint8_t tries = 0; tries < 64; tries++) {
        uint8_t cand_ip[16];
        memcpy(cand_ip, base_prefix, 16);

        uint32_t byte = (plen >= 32) ? 3 : 0;
        cand_ip[byte] = (uint8_t)(cand_ip[byte] ^ (uint8_t)(0xA5u + salt + tries));

        ipv6_prefix_mask(out_prefix, cand_ip, plen);

        if (ipv6_in_prefix(avoid_ip, out_prefix, plen)) {
            continue;
        }

        gboolean dup = FALSE;
        for (size_t i = 0; i < avoid_count; i++) {
            const uint8_t *ap = avoid_prefixes + i * 16;
            if (memcmp(ap, out_prefix, 16) == 0) {
                dup = TRUE;
                break;
            }
        }
        if (dup) continue;
        return;
    }
}


static inline uint32_t fnv1a32(const uint8_t *data, size_t len)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

static void derive_incident_tuple_v4(
    const uint8_t src4[4],
    uint32_t ingress_if,
    uint8_t dst4_out[4],
    uint16_t *src_port_out,
    uint16_t *dst_port_out,
    uint8_t *proto_out)
{
    (void)ingress_if;
    /* Victim set (documentation-reserved) */
    const uint8_t victim_N = 4;
    const uint8_t victim_index = (uint8_t)(src4[3] % victim_N);

    dst4_out[0] = 198;
    dst4_out[1] = 18;
    dst4_out[2] = victim_index;
    dst4_out[3] = 1;

    const uint32_t h = fnv1a32(src4, 4);
    *dst_port_out = (uint16_t)(10000u + (h % 1000u));
    *src_port_out = (uint16_t)(20000u + ((h >> 16) % 1000u));
    *proto_out = 6; /* TCP */
}

static void derive_incident_tuple_v6(
    const uint8_t src6[16],
    uint32_t ingress_if,
    uint8_t dst6_out[16],
    uint16_t *src_port_out,
    uint16_t *dst_port_out,
    uint8_t *proto_out)
{
    (void)ingress_if;
    const uint8_t victim_N = 4;
    const uint16_t tail16 = (uint16_t)(((uint16_t)src6[14] << 8) | src6[15]);
    const uint8_t victim_index = (uint8_t)(tail16 % victim_N);

    memset(dst6_out, 0, 16);
    /* Base: 2001:db8:ffff:<idx>::1 */
    dst6_out[0] = 0x20;
    dst6_out[1] = 0x01;
    dst6_out[2] = 0x0d;
    dst6_out[3] = 0xb8;
    dst6_out[4] = 0xff;
    dst6_out[5] = 0xff;
    dst6_out[6] = 0x00;
    dst6_out[7] = victim_index;
    dst6_out[15] = 0x01;

    const uint32_t h = fnv1a32(src6, 16);
    *dst_port_out = (uint16_t)(10000u + (h % 1000u));
    *src_port_out = (uint16_t)(20000u + ((h >> 16) % 1000u));
    *proto_out = 6; /* TCP */
}

/* ---- Aggregation keys ---- */

typedef struct sav_stl_sig {
    uint16_t sub_template_id;
    uint8_t semantic;
    uint32_t count;
    size_t entry_size;
    uint8_t entries[256];
} sav_stl_sig_t;

static gboolean build_export_stl_from_batch(
    const sav_packet_batch_t *b,
    uint8_t *out_semantic,
    uint16_t *out_sub_template_id,
    uint32_t *out_entry_count,
    size_t *out_entry_size,
    uint8_t out_entries[256],
    size_t *out_entries_len,
    GError **err)
{
    if (!b || !out_semantic || !out_sub_template_id || !out_entry_count || !out_entry_size || !out_entries || !out_entries_len) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Invalid parameters to build_export_stl_from_batch");
        return FALSE;
    }

    *out_sub_template_id = b->stl_sub_template_id;
    *out_entry_size = b->stl_entry_size;

    if (b->stl_entry_count == 0 || b->stl_entry_size == 0) {
        *out_semantic = b->stl_semantic;
        *out_entry_count = 0;
        *out_entries_len = 0;
        return TRUE;
    }

    /* Default: pass-through */
    *out_semantic = b->stl_semantic;
    *out_entry_count = b->stl_entry_count;
    size_t bytes = (size_t)b->stl_entry_count * b->stl_entry_size;
    if (bytes > sizeof(((sav_stl_sig_t *)0)->entries)) bytes = sizeof(((sav_stl_sig_t *)0)->entries);
    memcpy(out_entries, b->stl_entries, bytes);
    *out_entries_len = bytes;

    /* For allowlist, demonstrate multi-rule match list semantics, with constraints:
     * - semantic: allOf
     * - count: 3
     * - interface-based (900/901): all STL ingressInterface MUST equal packet ingressInterface;
     *   prefixes MUST be visibly different and MUST NOT cover the spoofed packet source IP.
     * - prefix-based (902/903): all STL sourcePrefix MUST equal packet-derived prefix;
     *   interfaces MUST be different and MUST NOT match the spoofed packet ingressInterface.
     */
    if (b->sav_rule_type != SAV_RULE_TYPE_ALLOWLIST) {
        return TRUE;
    }

    if (b->packet_count == 0) {
        return TRUE;
    }

    const sav_packet_t *p0 = &b->packets[0];

    const size_t one = b->stl_entry_size;
    if (one == 0 || one > 256) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "STL entry size invalid for allowlist demo");
        return FALSE;
    }

    if (one * 3 > 256) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "STL list too large for allowlist demo");
        return FALSE;
    }

    uint8_t first[256];
    memset(first, 0, sizeof(first));
    memcpy(first, b->stl_entries, one);

    /* Parse first entry (rule facts) */
    uint32_t rule_ingress = 0;
    uint8_t rule_p4[4];
    uint8_t rule_p6[16];
    uint8_t rule_plen = 0;

    if (*out_sub_template_id == SAV_TMPL_IPV4_INTERFACE_PREFIX) {
        memcpy(&rule_ingress, first + 0, 4);
        memcpy(rule_p4, first + 4, 4);
        rule_plen = first[8];
    } else if (*out_sub_template_id == SAV_TMPL_IPV4_PREFIX_INTERFACE) {
        memcpy(rule_p4, first + 0, 4);
        rule_plen = first[4];
        memcpy(&rule_ingress, first + 5, 4);
    } else if (*out_sub_template_id == SAV_TMPL_IPV6_INTERFACE_PREFIX) {
        memcpy(&rule_ingress, first + 0, 4);
        memcpy(rule_p6, first + 4, 16);
        rule_plen = first[20];
    } else if (*out_sub_template_id == SAV_TMPL_IPV6_PREFIX_INTERFACE) {
        memcpy(rule_p6, first + 0, 16);
        rule_plen = first[16];
        memcpy(&rule_ingress, first + 17, 4);
    } else {
        return TRUE;
    }

    /* Build 3 entries */
    uint8_t e1[256], e2[256], e3[256];
    memset(e1, 0, sizeof(e1));
    memset(e2, 0, sizeof(e2));
    memset(e3, 0, sizeof(e3));

    if (*out_sub_template_id == SAV_TMPL_IPV4_INTERFACE_PREFIX) {
        /* ingress must equal packet ingress */
        const uint32_t ingress = p0->ingress_interface;
        uint8_t pfx1[4];
        memcpy(pfx1, rule_p4, 4);

        uint8_t seen[12];
        memcpy(seen + 0, pfx1, 4);
        uint8_t pfx2[4];
        v4_make_alt_prefix_not_covering(pfx2, pfx1, rule_plen, p0->source_ip, seen, 1, 11);
        memcpy(seen + 4, pfx2, 4);
        uint8_t pfx3[4];
        v4_make_alt_prefix_not_covering(pfx3, pfx1, rule_plen, p0->source_ip, seen, 2, 29);

        memcpy(e1 + 0, &ingress, 4);
        memcpy(e1 + 4, pfx1, 4);
        e1[8] = rule_plen;

        memcpy(e2 + 0, &ingress, 4);
        memcpy(e2 + 4, pfx2, 4);
        e2[8] = rule_plen;

        memcpy(e3 + 0, &ingress, 4);
        memcpy(e3 + 4, pfx3, 4);
        e3[8] = rule_plen;
    } else if (*out_sub_template_id == SAV_TMPL_IPV6_INTERFACE_PREFIX) {
        const uint32_t ingress = p0->ingress_interface;
        uint8_t pfx1[16];
        memcpy(pfx1, rule_p6, 16);

        uint8_t seen[48];
        memcpy(seen + 0, pfx1, 16);
        uint8_t pfx2[16];
        v6_make_alt_prefix_not_covering(pfx2, pfx1, rule_plen, p0->source_ip, seen, 1, 7);
        memcpy(seen + 16, pfx2, 16);
        uint8_t pfx3[16];
        v6_make_alt_prefix_not_covering(pfx3, pfx1, rule_plen, p0->source_ip, seen, 2, 19);

        memcpy(e1 + 0, &ingress, 4);
        memcpy(e1 + 4, pfx1, 16);
        e1[20] = rule_plen;

        memcpy(e2 + 0, &ingress, 4);
        memcpy(e2 + 4, pfx2, 16);
        e2[20] = rule_plen;

        memcpy(e3 + 0, &ingress, 4);
        memcpy(e3 + 4, pfx3, 16);
        e3[20] = rule_plen;
    } else if (*out_sub_template_id == SAV_TMPL_IPV4_PREFIX_INTERFACE) {
        /* prefix must equal packet-derived prefix; ingress must differ from packet ingress */
        uint8_t pfx[4];
        ipv4_prefix_from_ip_net_host(pfx, p0->source_ip, rule_plen);

        const uint32_t base = rule_ingress;
        const uint32_t i1 = base;
        const uint32_t i2 = base + 1u;
        const uint32_t i3 = base + 2u;

        memcpy(e1 + 0, pfx, 4);
        e1[4] = rule_plen;
        memcpy(e1 + 5, &i1, 4);

        memcpy(e2 + 0, pfx, 4);
        e2[4] = rule_plen;
        memcpy(e2 + 5, &i2, 4);

        memcpy(e3 + 0, pfx, 4);
        e3[4] = rule_plen;
        memcpy(e3 + 5, &i3, 4);
    } else { /* SAV_TMPL_IPV6_PREFIX_INTERFACE */
        uint8_t pfx[16];
        ipv6_prefix_mask(pfx, p0->source_ip, rule_plen);

        const uint32_t base = rule_ingress;
        const uint32_t i1 = base;
        const uint32_t i2 = base + 1u;
        const uint32_t i3 = base + 2u;

        memcpy(e1 + 0, pfx, 16);
        e1[16] = rule_plen;
        memcpy(e1 + 17, &i1, 4);

        memcpy(e2 + 0, pfx, 16);
        e2[16] = rule_plen;
        memcpy(e2 + 17, &i2, 4);

        memcpy(e3 + 0, pfx, 16);
        e3[16] = rule_plen;
        memcpy(e3 + 17, &i3, 4);
    }

    memcpy(out_entries + 0 * one, e1, one);
    memcpy(out_entries + 1 * one, e2, one);
    memcpy(out_entries + 2 * one, e3, one);
    *out_entries_len = one * 3;
    *out_entry_count = 3;
    *out_semantic = SAV_STL_SEM_ALL_OF;
    return TRUE;
}

static GBytes* make_stl_sig_from_batch(const sav_packet_batch_t *b)
{
    sav_stl_sig_t sig;
    memset(&sig, 0, sizeof(sig));

    uint8_t semantic = 0;
    uint16_t sub_id = 0;
    uint32_t count = 0;
    size_t entry_size = 0;
    uint8_t entries[256];
    size_t entries_len = 0;
    GError *local_err = NULL;

    if (!build_export_stl_from_batch(b, &semantic, &sub_id, &count, &entry_size, entries, &entries_len, &local_err)) {
        /* Best-effort fallback: keep old signature to avoid crashing the exporter */
        if (local_err) g_error_free(local_err);
        sub_id = b->stl_sub_template_id;
        semantic = b->stl_semantic;
        count = b->stl_entry_count;
        entry_size = b->stl_entry_size;
        entries_len = (size_t)b->stl_entry_count * b->stl_entry_size;
        if (entries_len > sizeof(entries)) entries_len = sizeof(entries);
        memcpy(entries, b->stl_entries, entries_len);
    }

    sig.sub_template_id = sub_id;
    sig.semantic = semantic;
    sig.count = count;
    sig.entry_size = entry_size;
    if (entries_len > sizeof(sig.entries)) entries_len = sizeof(sig.entries);
    memcpy(sig.entries, entries, entries_len);

    /* Fixed-length serialization to keep hashing simple */
    return g_bytes_new(&sig, sizeof(sig));
}

typedef struct {
    uint64_t packet_count;
    uint64_t octet_count;
    uint64_t flow_start_ms;
    uint64_t flow_end_ms;
    uint8_t  policy_action;
} sav_agg_val_t;

typedef struct {
    sav_agg_val_t agg;
    const sav_packet_batch_t *rep_batch;
    const sav_packet_t *rep_pkt;
    uint8_t rule_type;
    uint8_t target_type;
} sav_view_val_t;

static void agg_add(sav_agg_val_t *v, const sav_packet_t *p)
{
    v->packet_count += 1;
    if (v->flow_start_ms == 0 || p->observation_time_ms < v->flow_start_ms) v->flow_start_ms = p->observation_time_ms;
    if (v->flow_end_ms == 0 || p->observation_time_ms > v->flow_end_ms) v->flow_end_ms = p->observation_time_ms;
    /* octet_count is a required field in exported records, but packet does not
     * carry packet length in this architecture. Use a fixed placeholder.
     */
    v->octet_count += 0;
}

/* ---- Export helpers ---- */

static gboolean stl_attach(
    fbSession_t *session,
    fbSubTemplateList_t *out_stl,
    const sav_packet_batch_t *b,
    gboolean expected_ipv6,
    GError **err)
{
    uint8_t semantic = 0;
    uint16_t sub_id = 0;
    uint32_t count = 0;
    size_t entry_size = 0;
    uint8_t entries[256];
    size_t entries_len = 0;

    if (!build_export_stl_from_batch(b, &semantic, &sub_id, &count, &entry_size, entries, &entries_len, err)) {
        return FALSE;
    }

    const gboolean sub_is_ipv6 = (sub_id == SAV_TMPL_IPV6_INTERFACE_PREFIX || sub_id == SAV_TMPL_IPV6_PREFIX_INTERFACE);
    const gboolean sub_is_ipv4 = (sub_id == SAV_TMPL_IPV4_INTERFACE_PREFIX || sub_id == SAV_TMPL_IPV4_PREFIX_INTERFACE);
    if ((!sub_is_ipv4 && !sub_is_ipv6) || (expected_ipv6 && !sub_is_ipv6) || (!expected_ipv6 && !sub_is_ipv4)) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Address family mismatch: record expects %s but SubTemplateList uses template %u",
                    expected_ipv6 ? "IPv6" : "IPv4",
                    (unsigned)sub_id);
        return FALSE;
    }

    fbTemplate_t *sub_tmpl = fbSessionGetTemplate(session, TRUE, sub_id, err);
    if (!sub_tmpl) {
        return FALSE;
    }

    fbSubTemplateListInit(out_stl,
                          semantic,
                          sub_id,
                          sub_tmpl,
                          count);

    if (count > 0) {
        void *ptr = fbSubTemplateListGetDataPtr(out_stl);
        if (!ptr) {
            g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "SubTemplateList data ptr NULL");
            fbSubTemplateListClear(out_stl);
            return FALSE;
        }
        (void)entry_size;
        memcpy(ptr, entries, entries_len);
    }

    return TRUE;
}

/* ---- Observation model export ---- */

gboolean sav_export_observation_models(
    const char                 *filename,
    const sav_packet_batch_t   *batches,
    size_t                      batch_count,
    GError                     **err)
{
    if (!filename || !batches) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Invalid parameters to sav_export_observation_models");
        return FALSE;
    }

    fbInfoModel_t *model = fbInfoModelAlloc();
    if (!model) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Failed to alloc info model");
        return FALSE;
    }
    sav_init_info_model(model);

    fbSession_t *session = fbSessionAlloc(model);
    if (!session) {
        fbInfoModelFree(model);
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Failed to alloc session");
        return FALSE;
    }

    if (!sav_add_templates(session, err)) {
        fbInfoModelFree(model);
        return FALSE;
    }

    fbExporter_t *exp = fbExporterAllocFile(filename);
    if (!exp) {
        fbInfoModelFree(model);
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Failed to create exporter for %s", filename);
        return FALSE;
    }

    fBuf_t *fbuf = fBufAllocForExport(session, exp);
    if (!fbuf) {
        fbInfoModelFree(model);
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Failed to allocate fBuf");
        return FALSE;
    }

    if (!fbSessionExportTemplates(session, err)) {
        fBufFree(fbuf);
        fbInfoModelFree(model);
        return FALSE;
    }

    const gboolean enable_template_b = env_enable_template_b();
    const gboolean enable_legacy_t123 = env_enable_legacy_t123();

    /* 1) T1 aggregation (Template 400): key = ingressInterface + ruleType + targetType + stl */
    GHashTable *t1 = g_hash_table_new_full(g_bytes_hash, g_bytes_equal, (GDestroyNotify)g_bytes_unref, g_free);

    /* 2) T2 aggregation (Template 410/420): key = ingressInterface + validation mode (+ stl)
     * Validation mode here means: ipVersion + ingressInterface + ruleType + targetType (+ STL signature).
     * NOTE: sourceIP is intentionally NOT part of the T2 key (coarser "which interface produces spoofed").
     */
    GHashTable *t2 = g_hash_table_new_full(g_bytes_hash, g_bytes_equal, (GDestroyNotify)g_bytes_unref, g_free);

    /* 3) T3 aggregation: key = sourcePrefix(/24 or /48) + ruleType + targetType + stl */
    GHashTable *t3 = g_hash_table_new_full(g_bytes_hash, g_bytes_equal, (GDestroyNotify)g_bytes_unref, g_free);

    /* Template A (Ops monitoring, default enabled)
     * Each flow answers: "某接口上，命中某一类 SAV 规则(集合)的 spoofed 流量汇总".
     */
    GHashTable *ta_v4 = g_hash_table_new_full(g_bytes_hash, g_bytes_equal, (GDestroyNotify)g_bytes_unref, g_free);
    GHashTable *ta_v6 = g_hash_table_new_full(g_bytes_hash, g_bytes_equal, (GDestroyNotify)g_bytes_unref, g_free);

    /* Template B (Incident investigation, disabled by default)
     * NOTE: Template B 不适合长期启用，仅用于事件分析（数据量大，粒度细）。
     */
    GHashTable *tb_v4 = enable_template_b ? g_hash_table_new_full(g_bytes_hash, g_bytes_equal, (GDestroyNotify)g_bytes_unref, g_free) : NULL;
    GHashTable *tb_v6 = enable_template_b ? g_hash_table_new_full(g_bytes_hash, g_bytes_equal, (GDestroyNotify)g_bytes_unref, g_free) : NULL;

    /* Demo mode: emit exactly 8 records:
     * - 4 allowlist records using Template A (500/501)
     * - 4 blocklist records using Template B (502/503)
     * and ALWAYS match address family:
     *   IPv4 templates only use subtemplates 900/902; IPv6 templates only use 901/903.
     */
    if (env_enable_template_a_crossproduct()) {
        const sav_packet_batch_t *allow_v4_ib = NULL;
        const sav_packet_batch_t *allow_v4_pb = NULL;
        const sav_packet_batch_t *allow_v6_ib = NULL;
        const sav_packet_batch_t *allow_v6_pb = NULL;

        const sav_packet_batch_t *block_v4_ib = NULL;
        const sav_packet_batch_t *block_v4_pb = NULL;
        const sav_packet_batch_t *block_v6_ib = NULL;
        const sav_packet_batch_t *block_v6_pb = NULL;

        for (size_t b = 0; b < batch_count; b++) {
            const sav_packet_batch_t *bat = &batches[b];
            if (bat->packet_count == 0) continue;

            if (bat->sav_rule_type == SAV_RULE_TYPE_ALLOWLIST) {
                if (bat->stl_sub_template_id == SAV_TMPL_IPV4_INTERFACE_PREFIX && !allow_v4_ib) allow_v4_ib = bat;
                if (bat->stl_sub_template_id == SAV_TMPL_IPV4_PREFIX_INTERFACE && !allow_v4_pb) allow_v4_pb = bat;
                if (bat->stl_sub_template_id == SAV_TMPL_IPV6_INTERFACE_PREFIX && !allow_v6_ib) allow_v6_ib = bat;
                if (bat->stl_sub_template_id == SAV_TMPL_IPV6_PREFIX_INTERFACE && !allow_v6_pb) allow_v6_pb = bat;
            } else if (bat->sav_rule_type == SAV_RULE_TYPE_BLOCKLIST) {
                if (bat->stl_sub_template_id == SAV_TMPL_IPV4_INTERFACE_PREFIX && !block_v4_ib) block_v4_ib = bat;
                if (bat->stl_sub_template_id == SAV_TMPL_IPV4_PREFIX_INTERFACE && !block_v4_pb) block_v4_pb = bat;
                if (bat->stl_sub_template_id == SAV_TMPL_IPV6_INTERFACE_PREFIX && !block_v6_ib) block_v6_ib = bat;
                if (bat->stl_sub_template_id == SAV_TMPL_IPV6_PREFIX_INTERFACE && !block_v6_pb) block_v6_pb = bat;
            }
        }

        if (!allow_v4_ib || !allow_v4_pb || !allow_v6_ib || !allow_v6_pb ||
            !block_v4_ib || !block_v4_pb || !block_v6_ib || !block_v6_pb) {
            g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                        "Demo requires allowlist+blocklist batches for 900/902/901/903");
            goto fail;
        }

        const sav_packet_t *allow_v4_ib_pkt = &allow_v4_ib->packets[0];
        const sav_packet_t *allow_v4_pb_pkt = &allow_v4_pb->packets[0];
        const sav_packet_t *allow_v6_ib_pkt = &allow_v6_ib->packets[0];
        const sav_packet_t *allow_v6_pb_pkt = &allow_v6_pb->packets[0];

        const sav_packet_t *block_v4_ib_pkt = &block_v4_ib->packets[0];
        const sav_packet_t *block_v4_pb_pkt = &block_v4_pb->packets[0];
        const sav_packet_t *block_v6_ib_pkt = &block_v6_ib->packets[0];
        const sav_packet_t *block_v6_pb_pkt = &block_v6_pb->packets[0];

        const uint16_t v4_sub_ids[2] = { SAV_TMPL_IPV4_INTERFACE_PREFIX, SAV_TMPL_IPV4_PREFIX_INTERFACE };
        const uint16_t v6_sub_ids[2] = { SAV_TMPL_IPV6_INTERFACE_PREFIX, SAV_TMPL_IPV6_PREFIX_INTERFACE };

        /* 2 records: Template A / IPv4 (allowlist) */
        for (int i = 0; i < 2; i++) {
            const uint16_t sub_id = v4_sub_ids[i];
            const sav_packet_batch_t *bat = (sub_id == SAV_TMPL_IPV4_INTERFACE_PREFIX) ? allow_v4_ib : allow_v4_pb;
            const sav_packet_t *pkt = (sub_id == SAV_TMPL_IPV4_INTERFACE_PREFIX) ? allow_v4_ib_pkt : allow_v4_pb_pkt;

            sav_template_a_v4_record_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.flowStartMilliseconds = pkt->observation_time_ms;
            rec.flowEndMilliseconds = pkt->observation_time_ms;
            rec.packetDeltaCount = 256;
            rec.octetDeltaCount = 0;
            rec.ingressInterface = pkt->ingress_interface;
            rec.savRuleType = SAV_RULE_TYPE_ALLOWLIST;
            rec.savTargetType = (sub_id == SAV_TMPL_IPV4_INTERFACE_PREFIX) ? SAV_TARGET_TYPE_INTERFACE_BASED : SAV_TARGET_TYPE_PREFIX_BASED;
            rec.savPolicyAction = SAV_POLICY_ACTION_DISCARD;

            uint8_t pref4_net[4];
            ipv4_prefix_mask(pref4_net, pkt->source_ip, SAV_T3_IPV4_PREFIXLEN);
            uint32_t pref_net_u = 0;
            memcpy(&pref_net_u, pref4_net, 4);
            rec.sourceIPv4Prefix = ntohl(pref_net_u);

            if (!stl_attach(session, &rec.savMatchedContentList, bat, FALSE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_TEMPLATE_A_IPV4, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        }

        /* 2 records: Template A / IPv6 (allowlist) */
        for (int i = 0; i < 2; i++) {
            const uint16_t sub_id = v6_sub_ids[i];
            const sav_packet_batch_t *bat = (sub_id == SAV_TMPL_IPV6_INTERFACE_PREFIX) ? allow_v6_ib : allow_v6_pb;
            const sav_packet_t *pkt = (sub_id == SAV_TMPL_IPV6_INTERFACE_PREFIX) ? allow_v6_ib_pkt : allow_v6_pb_pkt;

            sav_template_a_v6_record_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.flowStartMilliseconds = pkt->observation_time_ms;
            rec.flowEndMilliseconds = pkt->observation_time_ms;
            rec.packetDeltaCount = 256;
            rec.octetDeltaCount = 0;
            rec.ingressInterface = pkt->ingress_interface;
            rec.savRuleType = SAV_RULE_TYPE_ALLOWLIST;
            rec.savTargetType = (sub_id == SAV_TMPL_IPV6_INTERFACE_PREFIX) ? SAV_TARGET_TYPE_INTERFACE_BASED : SAV_TARGET_TYPE_PREFIX_BASED;
            rec.savPolicyAction = SAV_POLICY_ACTION_DISCARD;

            uint8_t pref6[16];
            ipv6_prefix_mask(pref6, pkt->source_ip, SAV_T3_IPV6_PREFIXLEN);
            memcpy(rec.sourceIPv6Prefix, pref6, 16);

            if (!stl_attach(session, &rec.savMatchedContentList, bat, TRUE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_TEMPLATE_A_IPV6, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        }

        /* 2 records: Template B / IPv4 (blocklist) */
        for (int i = 0; i < 2; i++) {
            const uint16_t sub_id = v4_sub_ids[i];
            const sav_packet_batch_t *bat = (sub_id == SAV_TMPL_IPV4_INTERFACE_PREFIX) ? block_v4_ib : block_v4_pb;
            const sav_packet_t *pkt = (sub_id == SAV_TMPL_IPV4_INTERFACE_PREFIX) ? block_v4_ib_pkt : block_v4_pb_pkt;

            sav_template_b_v4_record_t rec;
            memset(&rec, 0, sizeof(rec));

            uint32_t src_net = 0;
            memcpy(&src_net, pkt->source_ip, 4);
            rec.sourceIPv4Address = ntohl(src_net);

            uint8_t dst4[4];
            uint16_t sp = 0, dp = 0;
            uint8_t proto = 0;
            derive_incident_tuple_v4(pkt->source_ip, pkt->ingress_interface, dst4, &sp, &dp, &proto);
            uint32_t dst_net = 0;
            memcpy(&dst_net, dst4, 4);
            rec.destinationIPv4Address = ntohl(dst_net);
            rec.sourceTransportPort = sp;
            rec.destinationTransportPort = dp;
            rec.protocolIdentifier = proto;

            rec.ingressInterface = pkt->ingress_interface;
            rec.packetDeltaCount = 256;
            rec.flowStartMilliseconds = pkt->observation_time_ms;

            rec.savRuleType = SAV_RULE_TYPE_BLOCKLIST;
            rec.savTargetType = (sub_id == SAV_TMPL_IPV4_INTERFACE_PREFIX) ? SAV_TARGET_TYPE_INTERFACE_BASED : SAV_TARGET_TYPE_PREFIX_BASED;
            rec.savPolicyAction = SAV_POLICY_ACTION_DISCARD;

            if (!stl_attach(session, &rec.savMatchedContentList, bat, FALSE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_TEMPLATE_B_IPV4, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        }

        /* 2 records: Template B / IPv6 (blocklist) */
        for (int i = 0; i < 2; i++) {
            const uint16_t sub_id = v6_sub_ids[i];
            const sav_packet_batch_t *bat = (sub_id == SAV_TMPL_IPV6_INTERFACE_PREFIX) ? block_v6_ib : block_v6_pb;
            const sav_packet_t *pkt = (sub_id == SAV_TMPL_IPV6_INTERFACE_PREFIX) ? block_v6_ib_pkt : block_v6_pb_pkt;

            sav_template_b_v6_record_t rec;
            memset(&rec, 0, sizeof(rec));
            memcpy(rec.sourceIPv6Address, pkt->source_ip, 16);

            uint8_t dst6[16];
            uint16_t sp = 0, dp = 0;
            uint8_t proto = 0;
            derive_incident_tuple_v6(pkt->source_ip, pkt->ingress_interface, dst6, &sp, &dp, &proto);
            memcpy(rec.destinationIPv6Address, dst6, 16);
            rec.sourceTransportPort = sp;
            rec.destinationTransportPort = dp;
            rec.protocolIdentifier = proto;

            rec.ingressInterface = pkt->ingress_interface;
            rec.packetDeltaCount = 256;
            rec.flowStartMilliseconds = pkt->observation_time_ms;

            rec.savRuleType = SAV_RULE_TYPE_BLOCKLIST;
            rec.savTargetType = (sub_id == SAV_TMPL_IPV6_INTERFACE_PREFIX) ? SAV_TARGET_TYPE_INTERFACE_BASED : SAV_TARGET_TYPE_PREFIX_BASED;
            rec.savPolicyAction = SAV_POLICY_ACTION_DISCARD;

            if (!stl_attach(session, &rec.savMatchedContentList, bat, TRUE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_TEMPLATE_B_IPV6, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        }

        if (!fBufEmit(fbuf, err)) goto fail;
        /* Clean up and exit success path */
        g_hash_table_destroy(t1);
        g_hash_table_destroy(t2);
        g_hash_table_destroy(t3);
        g_hash_table_destroy(ta_v4);
        g_hash_table_destroy(ta_v6);
        if (tb_v4) g_hash_table_destroy(tb_v4);
        if (tb_v6) g_hash_table_destroy(tb_v6);
        fBufFree(fbuf);
        fbInfoModelFree(model);
        return TRUE;
    }

    for (size_t b = 0; b < batch_count; b++) {
        const sav_packet_batch_t *batch = &batches[b];
        GBytes *stl_sig = make_stl_sig_from_batch(batch);

        for (size_t i = 0; i < batch->packet_count; i++) {
            const sav_packet_t *p = &batch->packets[i];

            sav_ip_version_t ipv = (p->source_ip_len == 4) ? SAV_IP_V4 : SAV_IP_V6;

        /* T1 key */
        uint8_t t1_key_buf[32] = {0};
        size_t t1_off = 0;
        memcpy(t1_key_buf + t1_off, &p->ingress_interface, sizeof(p->ingress_interface));
        t1_off += sizeof(p->ingress_interface);
        t1_key_buf[t1_off++] = batch->sav_rule_type;
        t1_key_buf[t1_off++] = batch->sav_target_type;

        GByteArray *t1_key = g_byte_array_sized_new(128);
        g_byte_array_append(t1_key, t1_key_buf, (guint)t1_off);
        g_byte_array_append(t1_key, g_bytes_get_data(stl_sig, NULL), (guint)g_bytes_get_size(stl_sig));
        GBytes *t1_key_bytes = g_bytes_new(t1_key->data, t1_key->len);
        g_byte_array_free(t1_key, TRUE);

        sav_agg_val_t *v1 = g_hash_table_lookup(t1, t1_key_bytes);
        if (!v1) {
            v1 = g_malloc0(sizeof(*v1));
            g_hash_table_insert(t1, g_bytes_ref(t1_key_bytes), v1);
        }
        v1->policy_action = batch->sav_policy_action;
        agg_add(v1, p);
        g_bytes_unref(t1_key_bytes);

        /* T2 key */
        uint8_t t2_key_buf[64] = {0};
        size_t t2_off = 0;
        t2_key_buf[t2_off++] = (uint8_t)ipv;
        memcpy(t2_key_buf + t2_off, &p->ingress_interface, sizeof(p->ingress_interface));
        t2_off += sizeof(p->ingress_interface);
        t2_key_buf[t2_off++] = batch->sav_rule_type;
        t2_key_buf[t2_off++] = batch->sav_target_type;

        GByteArray *t2_key = g_byte_array_sized_new(128);
        g_byte_array_append(t2_key, t2_key_buf, (guint)t2_off);
        g_byte_array_append(t2_key, g_bytes_get_data(stl_sig, NULL), (guint)g_bytes_get_size(stl_sig));
        GBytes *t2_key_bytes = g_bytes_new(t2_key->data, t2_key->len);
        g_byte_array_free(t2_key, TRUE);

        sav_agg_val_t *v2 = g_hash_table_lookup(t2, t2_key_bytes);
        if (!v2) {
            v2 = g_malloc0(sizeof(*v2));
            g_hash_table_insert(t2, g_bytes_ref(t2_key_bytes), v2);
        }
        v2->policy_action = batch->sav_policy_action;
        agg_add(v2, p);
        g_bytes_unref(t2_key_bytes);

        /* T3 key */
        uint8_t t3_key_buf[64] = {0};
        size_t t3_off = 0;
        t3_key_buf[t3_off++] = (uint8_t)ipv;

        if (ipv == SAV_IP_V4) {
            uint8_t pref4[4];
            ipv4_prefix_mask(pref4, p->source_ip, SAV_T3_IPV4_PREFIXLEN);
            memcpy(t3_key_buf + t3_off, pref4, 4);
            t3_off += 4;
            t3_key_buf[t3_off++] = SAV_T3_IPV4_PREFIXLEN;
        } else {
            uint8_t pref6[16];
            ipv6_prefix_mask(pref6, p->source_ip, SAV_T3_IPV6_PREFIXLEN);
            memcpy(t3_key_buf + t3_off, pref6, 16);
            t3_off += 16;
            t3_key_buf[t3_off++] = SAV_T3_IPV6_PREFIXLEN;
        }

        t3_key_buf[t3_off++] = batch->sav_rule_type;
        t3_key_buf[t3_off++] = batch->sav_target_type;

        GByteArray *t3_key = g_byte_array_sized_new(128);
        g_byte_array_append(t3_key, t3_key_buf, (guint)t3_off);
        g_byte_array_append(t3_key, g_bytes_get_data(stl_sig, NULL), (guint)g_bytes_get_size(stl_sig));
        GBytes *t3_key_bytes = g_bytes_new(t3_key->data, t3_key->len);
        g_byte_array_free(t3_key, TRUE);

        sav_agg_val_t *v3 = g_hash_table_lookup(t3, t3_key_bytes);
        if (!v3) {
            v3 = g_malloc0(sizeof(*v3));
            g_hash_table_insert(t3, g_bytes_ref(t3_key_bytes), v3);
        }
        v3->policy_action = batch->sav_policy_action;
        agg_add(v3, p);
        g_bytes_unref(t3_key_bytes);

        /* ---- Template A view (ops monitoring) ----
         * Flow key: ingressInterface + savRuleType + savTargetType + savMatchedContent + sourcePrefix(derived)
         */
        if (ipv == SAV_IP_V4) {
            uint8_t pref4[4];
            ipv4_prefix_mask(pref4, p->source_ip, SAV_T3_IPV4_PREFIXLEN);

            uint8_t a_key_buf[64] = {0};
            size_t a_off = 0;
            memcpy(a_key_buf + a_off, &p->ingress_interface, sizeof(p->ingress_interface));
            a_off += sizeof(p->ingress_interface);
            a_key_buf[a_off++] = batch->sav_rule_type;
            a_key_buf[a_off++] = batch->sav_target_type;
            memcpy(a_key_buf + a_off, pref4, 4);
            a_off += 4;

            GByteArray *a_key = g_byte_array_sized_new(160);
            g_byte_array_append(a_key, a_key_buf, (guint)a_off);
            g_byte_array_append(a_key, g_bytes_get_data(stl_sig, NULL), (guint)g_bytes_get_size(stl_sig));
            GBytes *a_key_bytes = g_bytes_new(a_key->data, a_key->len);
            g_byte_array_free(a_key, TRUE);

            sav_view_val_t *va = g_hash_table_lookup(ta_v4, a_key_bytes);
            if (!va) {
                va = g_malloc0(sizeof(*va));
                va->rep_batch = batch;
                va->rep_pkt = p;
                va->rule_type = batch->sav_rule_type;
                va->target_type = batch->sav_target_type;
                g_hash_table_insert(ta_v4, g_bytes_ref(a_key_bytes), va);
            }
            va->agg.policy_action = batch->sav_policy_action;
            agg_add(&va->agg, p);
            g_bytes_unref(a_key_bytes);

            if (enable_template_b) {
                /* Template B (v4): 5-tuple derived deterministically from (sourceIP + ingressInterface). */
                uint8_t dst4[4];
                uint16_t sp, dp;
                uint8_t proto;
                derive_incident_tuple_v4(p->source_ip, p->ingress_interface, dst4, &sp, &dp, &proto);

                uint8_t b_key_buf[64] = {0};
                size_t b_off = 0;
                memcpy(b_key_buf + b_off, p->source_ip, 4);
                b_off += 4;
                memcpy(b_key_buf + b_off, dst4, 4);
                b_off += 4;
                memcpy(b_key_buf + b_off, &sp, 2);
                b_off += 2;
                memcpy(b_key_buf + b_off, &dp, 2);
                b_off += 2;
                b_key_buf[b_off++] = proto;
                memcpy(b_key_buf + b_off, &p->ingress_interface, sizeof(p->ingress_interface));
                b_off += sizeof(p->ingress_interface);

                GBytes *b_key_bytes = g_bytes_new(b_key_buf, b_off);
                sav_view_val_t *vb = g_hash_table_lookup(tb_v4, b_key_bytes);
                if (!vb) {
                    vb = g_malloc0(sizeof(*vb));
                    vb->rep_batch = batch;
                    vb->rep_pkt = p;
                    vb->rule_type = batch->sav_rule_type;
                    vb->target_type = batch->sav_target_type;
                    g_hash_table_insert(tb_v4, g_bytes_ref(b_key_bytes), vb);
                } else {
                    /* Template B flow key excludes SAV rule info.
                     * Ensure no accidental mixing across different rule contexts.
                     */
                    g_assert(vb->rep_batch == batch);
                }
                vb->agg.policy_action = batch->sav_policy_action;
                agg_add(&vb->agg, p);
                g_bytes_unref(b_key_bytes);
            }
        } else {
            uint8_t pref6[16];
            ipv6_prefix_mask(pref6, p->source_ip, SAV_T3_IPV6_PREFIXLEN);

            uint8_t a_key_buf[96] = {0};
            size_t a_off = 0;
            memcpy(a_key_buf + a_off, &p->ingress_interface, sizeof(p->ingress_interface));
            a_off += sizeof(p->ingress_interface);
            a_key_buf[a_off++] = batch->sav_rule_type;
            a_key_buf[a_off++] = batch->sav_target_type;
            memcpy(a_key_buf + a_off, pref6, 16);
            a_off += 16;

            GByteArray *a_key = g_byte_array_sized_new(200);
            g_byte_array_append(a_key, a_key_buf, (guint)a_off);
            g_byte_array_append(a_key, g_bytes_get_data(stl_sig, NULL), (guint)g_bytes_get_size(stl_sig));
            GBytes *a_key_bytes = g_bytes_new(a_key->data, a_key->len);
            g_byte_array_free(a_key, TRUE);

            sav_view_val_t *va = g_hash_table_lookup(ta_v6, a_key_bytes);
            if (!va) {
                va = g_malloc0(sizeof(*va));
                va->rep_batch = batch;
                va->rep_pkt = p;
                va->rule_type = batch->sav_rule_type;
                va->target_type = batch->sav_target_type;
                g_hash_table_insert(ta_v6, g_bytes_ref(a_key_bytes), va);
            }
            va->agg.policy_action = batch->sav_policy_action;
            agg_add(&va->agg, p);
            g_bytes_unref(a_key_bytes);

            if (enable_template_b) {
                uint8_t dst6[16];
                uint16_t sp, dp;
                uint8_t proto;
                derive_incident_tuple_v6(p->source_ip, p->ingress_interface, dst6, &sp, &dp, &proto);

                uint8_t b_key_buf[96] = {0};
                size_t b_off = 0;
                memcpy(b_key_buf + b_off, p->source_ip, 16);
                b_off += 16;
                memcpy(b_key_buf + b_off, dst6, 16);
                b_off += 16;
                memcpy(b_key_buf + b_off, &sp, 2);
                b_off += 2;
                memcpy(b_key_buf + b_off, &dp, 2);
                b_off += 2;
                b_key_buf[b_off++] = proto;
                memcpy(b_key_buf + b_off, &p->ingress_interface, sizeof(p->ingress_interface));
                b_off += sizeof(p->ingress_interface);

                GBytes *b_key_bytes = g_bytes_new(b_key_buf, b_off);
                sav_view_val_t *vb = g_hash_table_lookup(tb_v6, b_key_bytes);
                if (!vb) {
                    vb = g_malloc0(sizeof(*vb));
                    vb->rep_batch = batch;
                    vb->rep_pkt = p;
                    vb->rule_type = batch->sav_rule_type;
                    vb->target_type = batch->sav_target_type;
                    g_hash_table_insert(tb_v6, g_bytes_ref(b_key_bytes), vb);
                } else {
                    g_assert(vb->rep_batch == batch);
                }
                vb->agg.policy_action = batch->sav_policy_action;
                agg_add(&vb->agg, p);
                g_bytes_unref(b_key_bytes);
            }
        }

        }

        g_bytes_unref(stl_sig);
    }

    /* Export records from aggregation tables.
     * NOTE: We only guarantee exporter-side aggregation and template correctness.
     */

    /* ---- Template A export (Ops monitoring) ---- */
    GHashTableIter it_a;
    gpointer key_ptr_a, val_ptr_a;

    g_hash_table_iter_init(&it_a, ta_v4);
    while (g_hash_table_iter_next(&it_a, &key_ptr_a, &val_ptr_a)) {
        GBytes *k = key_ptr_a;
        const sav_view_val_t *v = val_ptr_a;
        const uint8_t *kb = g_bytes_get_data(k, NULL);

        uint32_t ingress_if;
        memcpy(&ingress_if, kb + 0, 4);
        uint8_t rule_type = kb[4];
        uint8_t target_type = kb[5];
        uint8_t pref4[4];
        memcpy(pref4, kb + 6, 4);

        sav_template_a_v4_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.flowStartMilliseconds = v->agg.flow_start_ms;
        rec.flowEndMilliseconds = v->agg.flow_end_ms;
        rec.packetDeltaCount = v->agg.packet_count;
        rec.octetDeltaCount = v->agg.octet_count;
        rec.ingressInterface = ingress_if;
        rec.savRuleType = rule_type;
        rec.savTargetType = target_type;
        rec.savPolicyAction = v->agg.policy_action;
        uint32_t pref_net = 0;
        memcpy(&pref_net, pref4, 4);
        rec.sourceIPv4Prefix = ntohl(pref_net);

        if (!stl_attach(session, &rec.savMatchedContentList, v->rep_batch, FALSE, err)) goto fail;
        if (!fBufSetTemplatesForExport(fbuf, SAV_TEMPLATE_A_IPV4, err)) {
            fbSubTemplateListClear(&rec.savMatchedContentList);
            goto fail;
        }
        if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
            fbSubTemplateListClear(&rec.savMatchedContentList);
            goto fail;
        }
        fbSubTemplateListClear(&rec.savMatchedContentList);
    }

    g_hash_table_iter_init(&it_a, ta_v6);
    while (g_hash_table_iter_next(&it_a, &key_ptr_a, &val_ptr_a)) {
        GBytes *k = key_ptr_a;
        const sav_view_val_t *v = val_ptr_a;
        const uint8_t *kb = g_bytes_get_data(k, NULL);

        uint32_t ingress_if;
        memcpy(&ingress_if, kb + 0, 4);
        uint8_t rule_type = kb[4];
        uint8_t target_type = kb[5];
        uint8_t pref6[16];
        memcpy(pref6, kb + 6, 16);

        sav_template_a_v6_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.flowStartMilliseconds = v->agg.flow_start_ms;
        rec.flowEndMilliseconds = v->agg.flow_end_ms;
        rec.packetDeltaCount = v->agg.packet_count;
        rec.octetDeltaCount = v->agg.octet_count;
        rec.ingressInterface = ingress_if;
        rec.savRuleType = rule_type;
        rec.savTargetType = target_type;
        rec.savPolicyAction = v->agg.policy_action;
        memcpy(rec.sourceIPv6Prefix, pref6, 16);

        if (!stl_attach(session, &rec.savMatchedContentList, v->rep_batch, TRUE, err)) goto fail;
        if (!fBufSetTemplatesForExport(fbuf, SAV_TEMPLATE_A_IPV6, err)) {
            fbSubTemplateListClear(&rec.savMatchedContentList);
            goto fail;
        }
        if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
            fbSubTemplateListClear(&rec.savMatchedContentList);
            goto fail;
        }
        fbSubTemplateListClear(&rec.savMatchedContentList);
    }

    /* ---- Template B export (Incident investigation; disabled by default) ---- */
    if (enable_template_b) {
        GHashTableIter it_b;
        gpointer key_ptr_b, val_ptr_b;

        g_hash_table_iter_init(&it_b, tb_v4);
        while (g_hash_table_iter_next(&it_b, &key_ptr_b, &val_ptr_b)) {
            GBytes *k = key_ptr_b;
            const sav_view_val_t *v = val_ptr_b;
            const uint8_t *kb = g_bytes_get_data(k, NULL);

            uint8_t src4[4], dst4[4];
            memcpy(src4, kb + 0, 4);
            memcpy(dst4, kb + 4, 4);
            uint16_t sp, dp;
            memcpy(&sp, kb + 8, 2);
            memcpy(&dp, kb + 10, 2);
            uint8_t proto = kb[12];
            uint32_t ingress_if;
            memcpy(&ingress_if, kb + 13, 4);

            sav_template_b_v4_record_t rec;
            memset(&rec, 0, sizeof(rec));
            uint32_t src_net = 0, dst_net = 0;
            memcpy(&src_net, src4, 4);
            memcpy(&dst_net, dst4, 4);
            rec.sourceIPv4Address = ntohl(src_net);
            rec.destinationIPv4Address = ntohl(dst_net);
            rec.sourceTransportPort = sp;
            rec.destinationTransportPort = dp;
            rec.protocolIdentifier = proto;
            rec.ingressInterface = ingress_if;
            rec.savRuleType = v->rule_type;
            rec.savTargetType = v->target_type;
            rec.savPolicyAction = v->agg.policy_action;
            rec.packetDeltaCount = v->agg.packet_count;
            rec.flowStartMilliseconds = v->agg.flow_start_ms;

            if (!stl_attach(session, &rec.savMatchedContentList, v->rep_batch, FALSE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_TEMPLATE_B_IPV4, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        }

        g_hash_table_iter_init(&it_b, tb_v6);
        while (g_hash_table_iter_next(&it_b, &key_ptr_b, &val_ptr_b)) {
            GBytes *k = key_ptr_b;
            const sav_view_val_t *v = val_ptr_b;
            const uint8_t *kb = g_bytes_get_data(k, NULL);

            const uint8_t *src6 = kb + 0;
            const uint8_t *dst6 = kb + 16;
            uint16_t sp, dp;
            memcpy(&sp, kb + 32, 2);
            memcpy(&dp, kb + 34, 2);
            uint8_t proto = kb[36];
            uint32_t ingress_if;
            memcpy(&ingress_if, kb + 37, 4);

            sav_template_b_v6_record_t rec;
            memset(&rec, 0, sizeof(rec));
            memcpy(rec.sourceIPv6Address, src6, 16);
            memcpy(rec.destinationIPv6Address, dst6, 16);
            rec.sourceTransportPort = sp;
            rec.destinationTransportPort = dp;
            rec.protocolIdentifier = proto;
            rec.ingressInterface = ingress_if;
            rec.savRuleType = v->rule_type;
            rec.savTargetType = v->target_type;
            rec.savPolicyAction = v->agg.policy_action;
            rec.packetDeltaCount = v->agg.packet_count;
            rec.flowStartMilliseconds = v->agg.flow_start_ms;

            if (!stl_attach(session, &rec.savMatchedContentList, v->rep_batch, TRUE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_TEMPLATE_B_IPV6, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        }
    }

    /* ---- Legacy observation model exports (T1/T2/T3) ----
     * Disabled by default. Enable only for backward-compat research runs:
     *   SAV_EXPORT_T123=1
     */
    if (enable_legacy_t123) {
    /* T1 export (Template 400) */
    GHashTableIter it;
    gpointer key_ptr, val_ptr;

    g_hash_table_iter_init(&it, t1);
    while (g_hash_table_iter_next(&it, &key_ptr, &val_ptr)) {
        GBytes *k = key_ptr;
        const sav_agg_val_t *v = val_ptr;
        const uint8_t *kb = g_bytes_get_data(k, NULL);

        uint32_t ingress_if;
        memcpy(&ingress_if, kb + 0, 4);
        uint8_t rule_type = kb[4];
        uint8_t target_type = kb[5];

        /* Reconstruct STL signature from packet (we need actual STL bytes); easiest:
         * find any packet matching this key by scanning once.
         * Given small N, O(N^2) is acceptable.
         */
        const sav_packet_batch_t *rep_batch = NULL;
        for (size_t b = 0; b < batch_count; b++) {
            const sav_packet_batch_t *batch = &batches[b];
            if (batch->sav_rule_type != rule_type || batch->sav_target_type != target_type) continue;
            GBytes *sig = make_stl_sig_from_batch(batch);
            size_t key_prefix_len = 4 + 2;
            gboolean sig_ok = (g_bytes_get_size(sig) == (g_bytes_get_size(k) - key_prefix_len) &&
                              memcmp(g_bytes_get_data(sig, NULL), kb + key_prefix_len, g_bytes_get_size(sig)) == 0);
            g_bytes_unref(sig);
            if (sig_ok) { rep_batch = batch; break; }
        }
        if (!rep_batch) {
            g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Internal error: representative packet not found for T1 key");
            goto fail;
        }

        sav_t1_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.flowStartMilliseconds = v->flow_start_ms;
        rec.flowEndMilliseconds = v->flow_end_ms;
        rec.packetDeltaCount = v->packet_count;
        rec.octetDeltaCount = v->octet_count;
        rec.ingressInterface = ingress_if;
        rec.savRuleType = rule_type;
        rec.savTargetType = target_type;
        rec.savPolicyAction = v->policy_action;

        if (rep_batch->packet_count == 0) {
            g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Internal error: representative batch has no packets for T1 key");
            goto fail;
        }
        const gboolean expected_ipv6 = (rep_batch->packets[0].source_ip_len == 16);
        if (!stl_attach(session, &rec.savMatchedContentList, rep_batch, expected_ipv6, err)) goto fail;
        if (!fBufSetTemplatesForExport(fbuf, SAV_T1_TEMPLATE, err)) {
            fbSubTemplateListClear(&rec.savMatchedContentList);
            goto fail;
        }
        if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
            fbSubTemplateListClear(&rec.savMatchedContentList);
            goto fail;
        }
        fbSubTemplateListClear(&rec.savMatchedContentList);
    }

    /* T2 export (Template 410/420) */
    g_hash_table_iter_init(&it, t2);
    while (g_hash_table_iter_next(&it, &key_ptr, &val_ptr)) {
        GBytes *k = key_ptr;
        const sav_agg_val_t *v = val_ptr;
        const uint8_t *kb = g_bytes_get_data(k, NULL);

        sav_ip_version_t ipv = (sav_ip_version_t)kb[0];
        uint32_t ingress_if;
        memcpy(&ingress_if, kb + 1, 4);
        uint8_t rule_type = kb[1 + 4 + 0];
        uint8_t target_type = kb[1 + 4 + 1];

        const sav_packet_batch_t *rep_batch = NULL;
        for (size_t b = 0; b < batch_count && !rep_batch; b++) {
            const sav_packet_batch_t *batch = &batches[b];
            if (batch->sav_rule_type != rule_type || batch->sav_target_type != target_type) continue;
            GBytes *sig = make_stl_sig_from_batch(batch);
            size_t key_prefix_len = 1 + 4 + 2;
            gboolean sig_ok = (g_bytes_get_size(sig) == (g_bytes_get_size(k) - key_prefix_len) &&
                              memcmp(g_bytes_get_data(sig, NULL), kb + key_prefix_len, g_bytes_get_size(sig)) == 0);
            g_bytes_unref(sig);
            if (!sig_ok) continue;
            rep_batch = batch;
        }
        if (!rep_batch) {
            g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Internal error: representative packet not found for T2 key");
            goto fail;
        }

        if (ipv == SAV_IP_V4) {
            sav_t2_record_v4_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.observationTimeMilliseconds = v->flow_start_ms;
            rec.flowStartMilliseconds = v->flow_start_ms;
            rec.flowEndMilliseconds = v->flow_end_ms;
            rec.packetDeltaCount = v->packet_count;
            rec.octetDeltaCount = v->octet_count;
            rec.ingressInterface = ingress_if;
            /* Coarse T2: source IP is not part of the flow key; export an unspecified address. */
            rec.sourceIPv4Address = 0;
            rec.savRuleType = rule_type;
            rec.savTargetType = target_type;
            rec.savPolicyAction = v->policy_action;

            if (!stl_attach(session, &rec.savMatchedContentList, rep_batch, FALSE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_T2_TEMPLATE_IPV4, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        } else {
            sav_t2_record_v6_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.observationTimeMilliseconds = v->flow_start_ms;
            rec.flowStartMilliseconds = v->flow_start_ms;
            rec.flowEndMilliseconds = v->flow_end_ms;
            rec.packetDeltaCount = v->packet_count;
            rec.octetDeltaCount = v->octet_count;
            rec.ingressInterface = ingress_if;
            /* Coarse T2: source IP is not part of the flow key; export an unspecified address. */
            memset(rec.sourceIPv6Address, 0, sizeof(rec.sourceIPv6Address));
            rec.savRuleType = rule_type;
            rec.savTargetType = target_type;
            rec.savPolicyAction = v->policy_action;

            if (!stl_attach(session, &rec.savMatchedContentList, rep_batch, TRUE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_T2_TEMPLATE_IPV6, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        }
    }

    /* T3 export: two templates (IPv4/IPv6) */
    g_hash_table_iter_init(&it, t3);
    while (g_hash_table_iter_next(&it, &key_ptr, &val_ptr)) {
        GBytes *k = key_ptr;
        const sav_agg_val_t *v = val_ptr;
        const uint8_t *kb = g_bytes_get_data(k, NULL);

        sav_ip_version_t ipv = (sav_ip_version_t)kb[0];
        size_t off = 1;

        uint8_t rule_type;
        uint8_t target_type;

        const sav_packet_batch_t *rep_batch = NULL;
        const sav_packet_t *rep_pkt = NULL;

        if (ipv == SAV_IP_V4) {
            uint8_t pref4[4];
            memcpy(pref4, kb + off, 4);
            off += 4;
            uint8_t plen = kb[off++];
            rule_type = kb[off++];
            target_type = kb[off++];

            for (size_t b = 0; b < batch_count && !rep_batch; b++) {
                const sav_packet_batch_t *batch = &batches[b];
                if (batch->sav_rule_type != rule_type || batch->sav_target_type != target_type) continue;
                GBytes *sig = make_stl_sig_from_batch(batch);
                size_t key_prefix_len = 1 + 4 + 1 + 2;
                gboolean sig_ok = (g_bytes_get_size(sig) == (g_bytes_get_size(k) - key_prefix_len) &&
                                  memcmp(g_bytes_get_data(sig, NULL), kb + key_prefix_len, g_bytes_get_size(sig)) == 0);
                g_bytes_unref(sig);
                if (!sig_ok) continue;
                for (size_t i = 0; i < batch->packet_count; i++) {
                    const sav_packet_t *p = &batch->packets[i];
                    if (p->source_ip_len != 4) continue;
                    uint8_t p_pref[4];
                    ipv4_prefix_mask(p_pref, p->source_ip, plen);
                    if (memcmp(p_pref, pref4, 4) != 0) continue;
                    rep_batch = batch;
                    rep_pkt = p;
                    break;
                }
            }

            if (!rep_batch || !rep_pkt) {
                g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Internal error: representative packet not found for T3v4 key");
                goto fail;
            }

            sav_t3_record_v4_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.flowStartMilliseconds = v->flow_start_ms;
            rec.flowEndMilliseconds = v->flow_end_ms;
            rec.packetDeltaCount = v->packet_count;
            rec.octetDeltaCount = v->octet_count;
            uint32_t pref_v4_net = 0;
            memcpy(&pref_v4_net, pref4, 4);
            rec.sourceIPv4Prefix = ntohl(pref_v4_net);
            rec.sourceIPv4PrefixLength = plen;
            rec.savRuleType = rule_type;
            rec.savTargetType = target_type;
            rec.savPolicyAction = v->policy_action;

            if (!stl_attach(session, &rec.savMatchedContentList, rep_batch, FALSE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_T3_TEMPLATE_IPV4, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        } else {
            uint8_t pref6[16];
            memcpy(pref6, kb + off, 16);
            off += 16;
            uint8_t plen = kb[off++];
            rule_type = kb[off++];
            target_type = kb[off++];

            for (size_t b = 0; b < batch_count && !rep_batch; b++) {
                const sav_packet_batch_t *batch = &batches[b];
                if (batch->sav_rule_type != rule_type || batch->sav_target_type != target_type) continue;
                GBytes *sig = make_stl_sig_from_batch(batch);
                size_t key_prefix_len = 1 + 16 + 1 + 2;
                gboolean sig_ok = (g_bytes_get_size(sig) == (g_bytes_get_size(k) - key_prefix_len) &&
                                  memcmp(g_bytes_get_data(sig, NULL), kb + key_prefix_len, g_bytes_get_size(sig)) == 0);
                g_bytes_unref(sig);
                if (!sig_ok) continue;
                for (size_t i = 0; i < batch->packet_count; i++) {
                    const sav_packet_t *p = &batch->packets[i];
                    if (p->source_ip_len != 16) continue;
                    uint8_t p_pref[16];
                    ipv6_prefix_mask(p_pref, p->source_ip, plen);
                    if (memcmp(p_pref, pref6, 16) != 0) continue;
                    rep_batch = batch;
                    rep_pkt = p;
                    break;
                }
            }

            if (!rep_batch || !rep_pkt) {
                g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Internal error: representative packet not found for T3v6 key");
                goto fail;
            }

            sav_t3_record_v6_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.flowStartMilliseconds = v->flow_start_ms;
            rec.flowEndMilliseconds = v->flow_end_ms;
            rec.packetDeltaCount = v->packet_count;
            rec.octetDeltaCount = v->octet_count;
            memcpy(rec.sourceIPv6Prefix, pref6, 16);
            rec.sourceIPv6PrefixLength = plen;
            rec.savRuleType = rule_type;
            rec.savTargetType = target_type;
            rec.savPolicyAction = v->policy_action;

            if (!stl_attach(session, &rec.savMatchedContentList, rep_batch, TRUE, err)) goto fail;
            if (!fBufSetTemplatesForExport(fbuf, SAV_T3_TEMPLATE_IPV6, err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
                fbSubTemplateListClear(&rec.savMatchedContentList);
                goto fail;
            }
            fbSubTemplateListClear(&rec.savMatchedContentList);
        }
    }
    } /* enable_legacy_t123 */

    if (!fBufEmit(fbuf, err)) {
        goto fail;
    }

    g_hash_table_destroy(t1);
    g_hash_table_destroy(t2);
    g_hash_table_destroy(t3);
    g_hash_table_destroy(ta_v4);
    g_hash_table_destroy(ta_v6);
    if (tb_v4) g_hash_table_destroy(tb_v4);
    if (tb_v6) g_hash_table_destroy(tb_v6);
    fBufFree(fbuf);
    fbInfoModelFree(model);
    return TRUE;

fail:
    if (t1) g_hash_table_destroy(t1);
    if (t2) g_hash_table_destroy(t2);
    if (t3) g_hash_table_destroy(t3);
    if (ta_v4) g_hash_table_destroy(ta_v4);
    if (ta_v6) g_hash_table_destroy(ta_v6);
    if (tb_v4) g_hash_table_destroy(tb_v4);
    if (tb_v6) g_hash_table_destroy(tb_v6);
    if (fbuf) fBufFree(fbuf);
    if (model) fbInfoModelFree(model);
    return FALSE;
}
