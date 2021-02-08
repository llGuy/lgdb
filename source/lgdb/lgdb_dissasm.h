#ifndef LGDB_DISSASM_H
#define LGDB_DISSASM_H

#include "lgdb_utility.h"

#include <Zydis/Decoder.h>
#include <Zydis/Formatter.h>

typedef ZydisDecodedInstruction lgdb_machine_instruction_t;

typedef struct lgdb_dissasm {
    ZydisDecoder decoder;
    ZydisFormatter formatter;
} lgdb_dissasm_t;

bool32_t lgdb_decode_instruction_at(lgdb_dissasm_t *dissasm, void *start, uint32_t len, lgdb_machine_instruction_t *dst);

#endif
