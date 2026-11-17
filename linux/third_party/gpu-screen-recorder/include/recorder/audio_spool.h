#ifndef GSR_RECORDER_AUDIO_SPOOL_H
#define GSR_RECORDER_AUDIO_SPOOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* The caller closes all muxers first and retains spools until this succeeds.
   Base timestamps are relative; stem timestamps use the capture clock. */
bool gsr_audio_spools_finalize(const char *base_filepath, const char **spool_paths,
    const char **names, size_t count, int64_t origin_us);
#endif
