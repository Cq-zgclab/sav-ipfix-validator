/**
 * @file sav_models.h
 * @brief Fixed SAV rule universe + exporter-side observation models (T1/T2/T3)
 */

#ifndef SAV_MODELS_H
#define SAV_MODELS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <fixbuf/public.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IP version discriminator */
typedef enum {
    SAV_IP_V4 = 4,
    SAV_IP_V6 = 6
} sav_ip_version_t;

/* A synthetic packet used as the ONLY fact source.
 * Context lock:
 * - packet carries ONLY sourceIP + ingressInterface + timestamp
 * - packet carries NO prefix/rule/inference semantics
 */
typedef struct sav_packet {
    uint32_t ingress_interface;
    uint64_t observation_time_ms;

    /* Source IP bytes.
     * IPv4 uses first 4 bytes, IPv6 uses all 16.
     */
    uint8_t  source_ip[16];
    uint8_t  source_ip_len; /* 4 or 16 */
} sav_packet_t;

/* A batch associates a packet stream with fixed metadata required to populate
 * SAV observation records. The metadata is NOT in packet.
 */
typedef struct sav_packet_batch {
    uint8_t sav_rule_type;     /* allowlist(0) / blocklist(1) */
    uint8_t sav_target_type;   /* interface-based(0) / prefix-based(1) */
    uint8_t sav_policy_action; /* permit(0) / discard(1) / ... */

    uint8_t  stl_semantic;        /* libfixbuf semantic (allOf=3, exactlyOneOf=1) */
    uint16_t stl_sub_template_id; /* 900-903 */
    uint32_t stl_entry_count;
    size_t   stl_entry_size;
    uint8_t  stl_entries[256];

    sav_packet_t *packets;
    size_t        packet_count;
} sav_packet_batch_t;

/* Export three independent observation models (T1/T2/T3) to a single IPFIX file.
 * Each model is aggregated directly from packets (no cross-template dependency).
 */
gboolean sav_export_observation_models(
    const char                 *filename,
    const sav_packet_batch_t   *batches,
    size_t                      batch_count,
    GError                     **err);

#ifdef __cplusplus
}
#endif

#endif /* SAV_MODELS_H */
