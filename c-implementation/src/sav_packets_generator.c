/**
 * @file sav_packets_generator.c
 * @brief Synthetic packet generator for SAV exporter-side model tests.
 */

#include "sav_packets_generator.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "sav_ie_definitions.h"

/* libfixbuf semantic values (observed convention)
 * - allOf: 3
 * - exactlyOneOf: 1
 */
#define SAV_STL_SEM_ALL_OF        3
#define SAV_STL_SEM_EXACTLY_ONEOF 1

static gboolean parse_ipv4(const char *s, uint8_t out4[4], GError **err)
{
    if (inet_pton(AF_INET, s, out4) != 1) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Invalid IPv4 address: %s", s);
        return FALSE;
    }
    return TRUE;
}

static gboolean parse_ipv6(const char *s, uint8_t out16[16], GError **err)
{
    if (inet_pton(AF_INET6, s, out16) != 1) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Invalid IPv6 address: %s", s);
        return FALSE;
    }
    return TRUE;
}

static gboolean stl_write_entry_ipv4_if_prefix(uint8_t *dst, size_t dst_len, uint32_t ingress_if, const uint8_t prefix4[4], uint8_t prefix_len, GError **err)
{
    if (dst_len < 9) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "STL buffer too small for IPv4 entry");
        return FALSE;
    }

    /* Store host-order integers; libfixbuf handles endian conversion for uint32 fields. */
    uint32_t iface_host = ingress_if;
    memcpy(dst + 0, &iface_host, 4);

    uint32_t pref_net = 0;
    memcpy(&pref_net, prefix4, 4);
    uint32_t pref_host = ntohl(pref_net);
    memcpy(dst + 4, &pref_host, 4);
    dst[8] = prefix_len;
    return TRUE;
}

static gboolean stl_write_entry_ipv6_if_prefix(uint8_t *dst, size_t dst_len, uint32_t ingress_if, const uint8_t prefix16[16], uint8_t prefix_len, GError **err)
{
    if (dst_len < 21) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "STL buffer too small for IPv6 entry");
        return FALSE;
    }

    /* Store host-order uint32; libfixbuf handles endian conversion. */
    uint32_t iface_host = ingress_if;
    memcpy(dst + 0, &iface_host, 4);
    memcpy(dst + 4, prefix16, 16);
    dst[20] = prefix_len;
    return TRUE;
}

static gboolean stl_write_entry_ipv4_prefix_if(uint8_t *dst, size_t dst_len, const uint8_t prefix4[4], uint8_t prefix_len, uint32_t ingress_if, GError **err)
{
    if (dst_len < 9) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "STL buffer too small for IPv4 entry");
        return FALSE;
    }

    uint32_t pref_net = 0;
    memcpy(&pref_net, prefix4, 4);
    uint32_t pref_host = ntohl(pref_net);
    memcpy(dst + 0, &pref_host, 4);
    dst[4] = prefix_len;
    uint32_t iface_host = ingress_if;
    memcpy(dst + 5, &iface_host, 4);
    return TRUE;
}

static gboolean stl_write_entry_ipv6_prefix_if(uint8_t *dst, size_t dst_len, const uint8_t prefix16[16], uint8_t prefix_len, uint32_t ingress_if, GError **err)
{
    if (dst_len < 21) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "STL buffer too small for IPv6 entry");
        return FALSE;
    }
    memcpy(dst + 0, prefix16, 16);
    dst[16] = prefix_len;
    uint32_t iface_host = ingress_if;
    memcpy(dst + 17, &iface_host, 4);
    return TRUE;
}

typedef struct {
    sav_rule_type_t   rule_type;   /* allowlist / blocklist */
    sav_target_type_t target_type; /* interface-based / prefix-based */
    uint32_t          ingress_if;
    uint8_t           prefix[16];
    uint8_t           prefix_len; /* bits */
    gboolean          is_ipv6;
} sav_rule_desc_t;

static void packet_set_raw(
    sav_packet_t *p,
    uint32_t ingress_if,
    const uint8_t *src_ip,
    uint8_t src_ip_len,
    uint64_t ts_ms)
{
    memset(p, 0, sizeof(*p));
    p->ingress_interface = ingress_if;
    p->observation_time_ms = ts_ms;
    p->source_ip_len = src_ip_len;
    memcpy(p->source_ip, src_ip, src_ip_len);
}

static inline uint32_t xorshift32(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

/* bit_index: 0 is MSB of byte[0] */
static void flip_msb0_bit(uint8_t *bytes, uint8_t bit_index)
{
    const uint8_t byte_index = (uint8_t)(bit_index / 8);
    const uint8_t bit_in_byte = (uint8_t)(bit_index % 8);
    const uint8_t mask = (uint8_t)(1u << (7u - bit_in_byte));
    bytes[byte_index] ^= mask;
}

static gboolean ip_matches_prefix(
    const uint8_t *ip,
    const uint8_t *prefix,
    uint8_t prefix_len,
    uint8_t ip_len)
{
    const uint16_t total_bits = (uint16_t)ip_len * 8u;
    if (prefix_len > total_bits) {
        return FALSE;
    }

    const uint8_t full_bytes = (uint8_t)(prefix_len / 8);
    const uint8_t rem_bits = (uint8_t)(prefix_len % 8);

    if (full_bytes > 0 && memcmp(ip, prefix, full_bytes) != 0) {
        return FALSE;
    }
    if (rem_bits == 0) {
        return TRUE;
    }

    const uint8_t mask = (uint8_t)(0xFFu << (8u - rem_bits));
    return (ip[full_bytes] & mask) == (prefix[full_bytes] & mask);
}

static void gen_ip_in_prefix(
    const sav_rule_desc_t *rule,
    uint32_t seq,
    uint8_t *out_ip,
    uint8_t *out_len)
{
    const uint8_t ip_len = rule->is_ipv6 ? 16 : 4;
    memcpy(out_ip, rule->prefix, ip_len);
    *out_len = ip_len;

    /* Fill host bits deterministically from seq so we can scale to 200-300 packets. */
    uint32_t x = xorshift32(seq ^ 0xA5A5A5A5u);
    const uint16_t total_bits = (uint16_t)ip_len * 8u;
    if (rule->prefix_len >= total_bits) {
        return;
    }

    const uint16_t host_start_bit = rule->prefix_len;
    const uint16_t host_bytes_start = host_start_bit / 8u;
    const uint8_t host_bit_offset = (uint8_t)(host_start_bit % 8u);

    /* If prefix_len is not byte-aligned, clear the remaining bits in that byte first. */
    if (host_bit_offset != 0 && host_bytes_start < ip_len) {
        const uint8_t keep_mask = (uint8_t)(0xFFu << (8u - host_bit_offset));
        out_ip[host_bytes_start] &= keep_mask;
        out_ip[host_bytes_start] |= (uint8_t)(x & (uint32_t)(~keep_mask));
        x = xorshift32(x);
    }

    for (uint16_t i = host_bytes_start + (host_bit_offset ? 1u : 0u); i < ip_len; i++) {
        out_ip[i] = (uint8_t)(x & 0xFFu);
        x = xorshift32(x);
    }
}

static void gen_ip_outside_prefix(
    const sav_rule_desc_t *rule,
    uint32_t seq,
    uint8_t *out_ip,
    uint8_t *out_len)
{
    gen_ip_in_prefix(rule, seq, out_ip, out_len);
    const uint16_t total_bits = (uint16_t)(*out_len) * 8u;
    if (rule->prefix_len >= total_bits) {
        /* Degenerate (should not happen for our fixed rules): flip the last bit anyway. */
        flip_msb0_bit(out_ip, (uint8_t)(total_bits - 1u));
        return;
    }

    /* Flip a bit *inside* the prefix portion to guarantee it's NOT in the prefix. */
    if (rule->prefix_len == 0) {
        /* Outside of /0 is impossible; keep as-is (should not exist in our fixed rules). */
        return;
    }
    flip_msb0_bit(out_ip, (uint8_t)(rule->prefix_len - 1u));
}

/* Generate packet facts (ingress_if + source_ip + ts) that are spoofed by rule semantics.
 * IMPORTANT: This does NOT perform SAV lookup; it only constructs inputs that violate the
 * specified rule model.
 */
static void gen_spoofed_packet_facts(
    const sav_rule_desc_t *rule,
    uint32_t seq,
    uint32_t *out_ingress_if,
    uint8_t *out_ip,
    uint8_t *out_ip_len)
{
    /* Spoofing semantics encoded explicitly:
     * - Allowlist, interface-based: use src ∈ prefix, but ingress ≠ allowlisted interface.
     * - Allowlist, prefix-based:    use src ∉ allowed prefix.
     * - Blocklist, interface-based: use ingress = blocked interface.
     * - Blocklist, prefix-based:    use src ∈ blocked prefix.
     */
    /*
     * IMPORTANT: Allowlist spoofing semantics for this project:
     * - interface-based allowlist: ingress matches, but source prefix mismatches (src ∉ allowlisted prefix)
     * - prefix-based allowlist:    source prefix matches (src ∈ allowlisted prefix), but ingress mismatches
     */
    if (rule->rule_type == SAV_RULE_TYPE_ALLOWLIST && rule->target_type == SAV_TARGET_TYPE_INTERFACE_BASED) {
        gen_ip_outside_prefix(rule, seq, out_ip, out_ip_len);
        *out_ingress_if = rule->ingress_if;
        return;
    }

    if (rule->rule_type == SAV_RULE_TYPE_ALLOWLIST && rule->target_type == SAV_TARGET_TYPE_PREFIX_BASED) {
        gen_ip_in_prefix(rule, seq, out_ip, out_ip_len);
        *out_ingress_if = rule->ingress_if + 100u; /* guaranteed different for our fixed universe */
        return;
    }

    if (rule->rule_type == SAV_RULE_TYPE_BLOCKLIST && rule->target_type == SAV_TARGET_TYPE_INTERFACE_BASED) {
        gen_ip_in_prefix(rule, seq, out_ip, out_ip_len);
        *out_ingress_if = rule->ingress_if;
        return;
    }

    /* Blocklist + prefix-based */
    gen_ip_in_prefix(rule, seq, out_ip, out_ip_len);
    *out_ingress_if = rule->ingress_if;
}

gboolean sav_generate_spoofed_packet_batches(
    sav_packet_batch_t **out_batches,
    size_t             *out_batch_count,
    GError             **err)
{
    if (!out_batches || !out_batch_count) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "NULL out parameter");
        return FALSE;
    }

    const size_t total_batches = 8;

    /* Rule-driven generator:
     * - Exactly 8 predefined SAV rules (IPv4/IPv6 × allowlist/blocklist × interface/prefix).
     * - For each rule, generate 200–300 spoofed packets derived from that rule's semantics.
     * - Timestamps must be globally monotonic and within ~1 second window per rule batch.
     */
    const size_t packets_per_rule = 256; /* within [200, 300] */

    sav_packet_batch_t *batches = g_malloc0(sizeof(*batches) * total_batches);
    if (!batches) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "OOM allocating batches");
        return FALSE;
    }

    uint64_t t0 = 1735171200000ULL; /* 2025-12-26T00:00:00Z in ms (fixed) */
    const uint64_t dt_ms = 3ULL;
    size_t ts_idx = 0;

    /* The generator emits ONLY spoofed packets.
     * Downstream export must always encode savPolicyAction=discard.
     */
    if (dt_ms * (packets_per_rule - 1) >= 1000ULL) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Timestamp spacing too large for per-rule ~1s window");
        g_free(batches);
        return FALSE;
    }

    /* Prefixes for the fixed rule universe (parse once; then copied into rules/STL). */
    uint8_t pfx_v4_allow_ib[4];
    uint8_t pfx_v4_allow_pb[4];
    uint8_t pfx_v4_block_ib[4];
    uint8_t pfx_v4_block_pb[4];

    uint8_t pfx_v6_allow_ib[16];
    uint8_t pfx_v6_allow_pb[16];
    uint8_t pfx_v6_block_ib[16];
    uint8_t pfx_v6_block_pb[16];

    if (!parse_ipv4("10.0.0.0", pfx_v4_allow_ib, err) ||
        !parse_ipv4("192.0.2.0", pfx_v4_allow_pb, err) ||
        !parse_ipv4("203.0.113.0", pfx_v4_block_ib, err) ||
        !parse_ipv4("198.51.100.0", pfx_v4_block_pb, err) ||
        !parse_ipv6("2001:db8:1::", pfx_v6_allow_ib, err) ||
        !parse_ipv6("2001:db8:100::", pfx_v6_allow_pb, err) ||
        !parse_ipv6("2001:db8:abcd::", pfx_v6_block_ib, err) ||
        !parse_ipv6("2001:db8:dcba::", pfx_v6_block_pb, err)) {
        g_free(batches);
        return FALSE;
    }

    /* Exactly 8 predefined rules.
     * Each batch corresponds to exactly one rule (traceability requirement).
     */
    const sav_rule_desc_t rules[8] = {
        /* IPv4 allowlist interface-based (allowed: IF 5001 for 10.0.0.0/24)
         * Spoofed condition: src ∈ 10.0.0.0/24 but ingress ≠ 5001.
         */
        { SAV_RULE_TYPE_ALLOWLIST, SAV_TARGET_TYPE_INTERFACE_BASED, 5001, {0}, 24, FALSE },

        /* IPv4 allowlist prefix-based (allowed prefix: 192.0.2.0/24)
         * Spoofed condition: src ∉ 192.0.2.0/24.
         */
        { SAV_RULE_TYPE_ALLOWLIST, SAV_TARGET_TYPE_PREFIX_BASED,    5002, {0}, 24, FALSE },

        /* IPv4 blocklist interface-based (blocked interface: 5001)
         * Spoofed condition: ingress = 5001.
         */
        { SAV_RULE_TYPE_BLOCKLIST, SAV_TARGET_TYPE_INTERFACE_BASED, 5001, {0}, 24, FALSE },

        /* IPv4 blocklist prefix-based (blocked prefix: 198.51.100.0/24)
         * Spoofed condition: src ∈ 198.51.100.0/24.
         */
        { SAV_RULE_TYPE_BLOCKLIST, SAV_TARGET_TYPE_PREFIX_BASED,    5005, {0}, 24, FALSE },

        /* IPv6 allowlist interface-based (allowed: IF 6001 for 2001:db8:1::/48)
         * Spoofed condition: src ∈ 2001:db8:1::/48 but ingress ≠ 6001.
         */
        { SAV_RULE_TYPE_ALLOWLIST, SAV_TARGET_TYPE_INTERFACE_BASED, 6001, {0}, 48, TRUE },

        /* IPv6 allowlist prefix-based (allowed prefix: 2001:db8:100::/48)
         * Spoofed condition: src ∉ 2001:db8:100::/48.
         */
        { SAV_RULE_TYPE_ALLOWLIST, SAV_TARGET_TYPE_PREFIX_BASED,    6002, {0}, 48, TRUE },

        /* IPv6 blocklist interface-based (blocked interface: 6003)
         * Spoofed condition: ingress = 6003.
         */
        { SAV_RULE_TYPE_BLOCKLIST, SAV_TARGET_TYPE_INTERFACE_BASED, 6003, {0}, 48, TRUE },

        /* IPv6 blocklist prefix-based (blocked prefix: 2001:db8:dcba::/48)
         * Spoofed condition: src ∈ 2001:db8:dcba::/48.
         */
        { SAV_RULE_TYPE_BLOCKLIST, SAV_TARGET_TYPE_PREFIX_BASED,    6004, {0}, 48, TRUE },
    };

    /* Copy parsed prefixes into the fixed rules (in declaration order above). */
    sav_rule_desc_t rules_mut[8];
    memcpy(rules_mut, rules, sizeof(rules_mut));

    memcpy(rules_mut[0].prefix, pfx_v4_allow_ib, 4);
    memcpy(rules_mut[1].prefix, pfx_v4_allow_pb, 4);
    memcpy(rules_mut[2].prefix, pfx_v4_block_ib, 4);
    memcpy(rules_mut[3].prefix, pfx_v4_block_pb, 4);
    memcpy(rules_mut[4].prefix, pfx_v6_allow_ib, 16);
    memcpy(rules_mut[5].prefix, pfx_v6_allow_pb, 16);
    memcpy(rules_mut[6].prefix, pfx_v6_block_ib, 16);
    memcpy(rules_mut[7].prefix, pfx_v6_block_pb, 16);

    for (size_t b = 0; b < total_batches; b++) {
        const sav_rule_desc_t *r = &rules_mut[b];

        batches[b].sav_rule_type = (uint8_t)r->rule_type;
        batches[b].sav_target_type = (uint8_t)r->target_type;
        batches[b].sav_policy_action = SAV_POLICY_ACTION_DISCARD;

        /* Provide per-rule metadata to populate savMatchedContentList downstream.
         * This generator does not build IPFIX/STL objects; it only provides bytes.
         */
        batches[b].stl_semantic = SAV_STL_SEM_EXACTLY_ONEOF;
        batches[b].stl_entry_count = 1;

        if (!r->is_ipv6 && r->target_type == SAV_TARGET_TYPE_INTERFACE_BASED) {
            batches[b].stl_sub_template_id = SAV_TMPL_IPV4_INTERFACE_PREFIX;
            batches[b].stl_entry_size = 9;
            if (!stl_write_entry_ipv4_if_prefix(batches[b].stl_entries, sizeof(batches[b].stl_entries), r->ingress_if, r->prefix, r->prefix_len, err)) {
                sav_free_packet_batches(batches, total_batches);
                return FALSE;
            }
        } else if (r->is_ipv6 && r->target_type == SAV_TARGET_TYPE_INTERFACE_BASED) {
            batches[b].stl_sub_template_id = SAV_TMPL_IPV6_INTERFACE_PREFIX;
            batches[b].stl_entry_size = 21;
            if (!stl_write_entry_ipv6_if_prefix(batches[b].stl_entries, sizeof(batches[b].stl_entries), r->ingress_if, r->prefix, r->prefix_len, err)) {
                sav_free_packet_batches(batches, total_batches);
                return FALSE;
            }
        } else if (!r->is_ipv6 && r->target_type == SAV_TARGET_TYPE_PREFIX_BASED) {
            batches[b].stl_sub_template_id = SAV_TMPL_IPV4_PREFIX_INTERFACE;
            batches[b].stl_entry_size = 9;
            if (!stl_write_entry_ipv4_prefix_if(batches[b].stl_entries, sizeof(batches[b].stl_entries), r->prefix, r->prefix_len, r->ingress_if, err)) {
                sav_free_packet_batches(batches, total_batches);
                return FALSE;
            }
        } else {
            batches[b].stl_sub_template_id = SAV_TMPL_IPV6_PREFIX_INTERFACE;
            batches[b].stl_entry_size = 21;
            if (!stl_write_entry_ipv6_prefix_if(batches[b].stl_entries, sizeof(batches[b].stl_entries), r->prefix, r->prefix_len, r->ingress_if, err)) {
                sav_free_packet_batches(batches, total_batches);
                return FALSE;
            }
        }
    }

    /* Allocate + fill packets per batch */
    for (size_t b = 0; b < total_batches; b++) {
        batches[b].packets = g_malloc0(sizeof(sav_packet_t) * packets_per_rule);
        if (!batches[b].packets) {
            g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "OOM allocating batch packets");
            sav_free_packet_batches(batches, total_batches);
            return FALSE;
        }
        batches[b].packet_count = packets_per_rule;

        const sav_rule_desc_t *r = &rules_mut[b];

        /*
         * IMPORTANT for Template B aggregation:
         * Template B derives the 5-tuple deterministically from (sourceIP + ingressInterface).
         * If we generate a unique sourceIP per packet, Template B will output 1 packet per flow.
         *
         * To exercise multi-packet-per-flow behavior, we intentionally reuse a small number of
         * distinct spoofed packet "facts" (sourceIP + ingress_if) per rule batch.
         *
         * With packets_per_rule=256 and flows_per_rule=5 => each flow aggregates ~51-52 packets.
         * This satisfies the requirement of ~50-100 packets per Template B flow record for all
         * scenarios (IPv4/IPv6, interface-based/prefix-based).
         */
        const uint32_t flows_per_rule = 5u;
        g_assert(flows_per_rule > 0);
        g_assert((packets_per_rule / flows_per_rule) >= 50u);
        g_assert((packets_per_rule / flows_per_rule) <= 100u);

        for (uint32_t seq = 0; seq < (uint32_t)packets_per_rule; seq++) {
            uint8_t ip[16];
            uint8_t ip_len = 0;
            uint32_t ingress_if = 0;

            const uint32_t flow_seq = (seq % flows_per_rule) + (uint32_t)(b * flows_per_rule);
            gen_spoofed_packet_facts(r, flow_seq, &ingress_if, ip, &ip_len);

            /* Assertions (no lookup): verify the spoofing predicate we encoded. */
            const gboolean in_prefix = ip_matches_prefix(ip, r->prefix, r->prefix_len, ip_len);
            if (r->rule_type == SAV_RULE_TYPE_ALLOWLIST && r->target_type == SAV_TARGET_TYPE_INTERFACE_BASED) {
                g_assert(!in_prefix);
                g_assert(ingress_if == r->ingress_if);
            } else if (r->rule_type == SAV_RULE_TYPE_ALLOWLIST && r->target_type == SAV_TARGET_TYPE_PREFIX_BASED) {
                g_assert(in_prefix);
                g_assert(ingress_if != r->ingress_if);
            } else if (r->rule_type == SAV_RULE_TYPE_BLOCKLIST && r->target_type == SAV_TARGET_TYPE_INTERFACE_BASED) {
                g_assert(ingress_if == r->ingress_if);
            } else {
                g_assert(in_prefix);
                g_assert(ingress_if == r->ingress_if);
            }

            const uint64_t ts = t0 + (uint64_t)(ts_idx++ * dt_ms);
            packet_set_raw(&batches[b].packets[seq], ingress_if, ip, ip_len, ts);
        }
    }

    *out_batches = batches;
    *out_batch_count = total_batches;
    return TRUE;
}

void sav_free_packet_batches(
    sav_packet_batch_t *batches,
    size_t              batch_count)
{
    if (!batches) return;
    for (size_t i = 0; i < batch_count; i++) {
        if (batches[i].packets) {
            g_free(batches[i].packets);
            batches[i].packets = NULL;
        }
        batches[i].packet_count = 0;
    }
    g_free(batches);
}
