#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "sav_packets_generator.h"
#include "sav_models.h"

#define IPFIX_FILE "test_sav_e2e.ipfix"

int main(void)
{
    printf("\n╔═════════════════════════════════════════════════════╗\n");
    printf("║   SAV IPFIX Exporter-side Models E2E (libfixbuf)   ║\n");
    printf("╚═════════════════════════════════════════════════════╝\n\n");

    GError *err = NULL;
    sav_packet_batch_t *batches = NULL;
    size_t batch_count = 0;

    if (!sav_generate_spoofed_packet_batches(&batches, &batch_count, &err)) {
        fprintf(stderr, "❌ Packet generation failed: %s\n", err ? err->message : "unknown");
        if (err) g_error_free(err);
        return 1;
    }

    size_t packet_total = 0;
    for (size_t i = 0; i < batch_count; i++) {
        packet_total += batches[i].packet_count;
    }
    printf("[OK] Generated %zu semantic-consistent spoofed packets\n", packet_total);

    if (!sav_export_observation_models(IPFIX_FILE, batches, batch_count, &err)) {
        fprintf(stderr, "❌ Export failed: %s\n", err ? err->message : "unknown");
        if (err) g_error_free(err);
        sav_free_packet_batches(batches, batch_count);
        return 1;
    }

    printf("[OK] Exported story templates (A/B) to %s\n", IPFIX_FILE);
    printf("\n✅ E2E EXPORT PASSED\n");

    sav_free_packet_batches(batches, batch_count);
    return 0;
}
