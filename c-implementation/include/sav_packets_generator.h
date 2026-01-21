/**
 * @file sav_packets_generator.h
 * @brief Synthetic packet generator for SAV exporter-side model tests.
 *
 * This module generates input packets (Context Lock facts only) plus the
 * per-batch metadata needed to populate savMatchedContentList.
 *
 * Design intent:
 * - Keep scenario/test-data iteration isolated from aggregation/export logic.
 * - Packets do NOT carry rule/inference semantics.
 */

#ifndef SAV_PACKETS_GENERATOR_H
#define SAV_PACKETS_GENERATOR_H

#include <stddef.h>

#include <glib.h>

#include "sav_models.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate a fixed set of spoofed packets grouped into batches.
 * Each batch provides the fixed metadata needed to export IPFIX observation
 * records without embedding semantics in packets.
 */
gboolean sav_generate_spoofed_packet_batches(
    sav_packet_batch_t **out_batches,
    size_t             *out_batch_count,
    GError             **err);

/** Free batches allocated by sav_generate_spoofed_packet_batches(). */
void sav_free_packet_batches(
    sav_packet_batch_t *batches,
    size_t              batch_count);

#ifdef __cplusplus
}
#endif

#endif /* SAV_PACKETS_GENERATOR_H */
