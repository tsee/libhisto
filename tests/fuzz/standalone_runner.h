#ifndef HISTO_STANDALONE_RUNNER_H
#define HISTO_STANDALONE_RUNNER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Target function interface required by libFuzzer and the standalone runner */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* HISTO_STANDALONE_RUNNER_H */
