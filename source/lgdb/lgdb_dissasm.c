#include "lgdb_dissasm.h"


bool32_t lgdb_decode_instruction_at(
    lgdb_dissasm_t *dissasm,
    void *start,
    uint32_t len,
    lgdb_machine_instruction_t *dst) {
    if (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&dissasm->decoder, start, len, dst))) {
        return 1;
    }
    else {
        return 0;
    }
}
