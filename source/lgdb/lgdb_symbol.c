#define _NO_CVCONST_H

#include <stdio.h>
#include <Windows.h>
#include <DbgHelp.h>
#include "lgdb_symbol.h"
#include "lgdb_context.h"

#include <stdbool.h>
#include "TypeInfoStructs.h"


IMAGEHLP_SYMBOL64 *lgdb_make_symbol_info(
    struct lgdb_process_ctx *ctx,
    const char *symbol_name) {
    IMAGEHLP_SYMBOL64 *symbol = (IMAGEHLP_SYMBOL64 *)lgdb_lnmalloc(&ctx->lnmem, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);

    memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);
    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength = MAX_SYM_NAME;

    BOOL success = WIN32_CALL(
        SymGetSymFromName64,
        ctx->proc_info.hProcess,
        symbol_name,
        symbol);

    if (!success) {
        free(symbol);
        symbol = NULL;
    }

    return symbol;
}


IMAGEHLP_LINE64 lgdb_make_line_info(
    struct lgdb_process_ctx *ctx,
    const char *file_name,
    uint32_t line_number) {
    IMAGEHLP_LINE64 line = {
        .SizeOfStruct = sizeof(line)
    };

    uint32_t displacement;
    BOOL success = WIN32_CALL(SymGetLineFromName64,
        ctx->proc_info.hProcess,
        ctx->module_info.ModuleName,
        file_name,
        line_number,
        &displacement,
        &line);

    if (!success) {
        memset(&line, 0, sizeof(line));
    }

    return line;
}


IMAGEHLP_LINE64 lgdb_make_line_info_from_addr(struct lgdb_process_ctx *ctx, void *addr) {
    IMAGEHLP_LINE64 line = {
        .SizeOfStruct = sizeof(line)
    };

    uint32_t displacement;
    BOOL success = WIN32_CALL(
        SymGetLineFromAddr64,
        ctx->proc_info.hProcess,
        (DWORD64)addr,
        &displacement,
        &line);

    if (!success) {
        memset(&line, 0, sizeof(line));
    }

    return line;
}


IMAGEHLP_LINE64 lgdb_get_next_line_info(struct lgdb_process_ctx *ctx, IMAGEHLP_LINE64 line) {
    WIN32_CALL(SymGetLineNext64, ctx->proc_info.hProcess, &line);
    return line;
}


void lgdb_update_symbol_context(struct lgdb_process_ctx *ctx) {
    STACKFRAME64 frame;
    lgdb_get_stack_frame(ctx, &frame);

    IMAGEHLP_STACK_FRAME stack_frame = {
        .InstructionOffset = ctx->thread_ctx.Rip,
        .ReturnOffset = frame.AddrReturn.Offset,
        .FrameOffset = ctx->thread_ctx.Rbp,
        .StackOffset = ctx->thread_ctx.Rsp,
        .Virtual = FALSE
    };

    SymSetContext(ctx->proc_info.hProcess, &stack_frame, NULL);
}


static void s_print_symbol_flags(uint32_t flags) {
    if (flags & SYMFLAG_CLR_TOKEN)
        printf("The symbol is a CLR token.\n");
    if (flags & SYMFLAG_CONSTANT)
        printf("The symbol is a constant.\n");
    if (flags & SYMFLAG_EXPORT)
        printf("The symbol is from the export table.\n");
    if (flags & SYMFLAG_FORWARDER)
        printf("The symbol is a forwarder.\n");
    if (flags & SYMFLAG_FRAMEREL)
        printf("Offsets are frame relative.\n");
    if (flags & SYMFLAG_FUNCTION)
        printf("The symbol is a known function.\n");
    if (flags & SYMFLAG_ILREL)
        printf("The symbol address is an offset relative to the beginning of the intermediate language block. This applies to managed code only.\n");
    if (flags & SYMFLAG_LOCAL)
        printf("The symbol is a local variable.\n");
    if (flags & SYMFLAG_METADATA)
        printf("The symbol is managed metadata.\n");
    if (flags & SYMFLAG_PARAMETER)
        printf("The symbol is a parameter.\n");
    if (flags & SYMFLAG_REGISTER)
        printf("The symbol is a register. The Register member is used.\n");
    if (flags & SYMFLAG_REGREL)
        printf("Offsets are register relative.\n");
    if (flags & SYMFLAG_SLOT)
        printf("The symbol is a managed code slot.\n");
    if (flags & SYMFLAG_THUNK)
        printf("The symbol is a thunk.\n");
    if (flags &SYMFLAG_TLSREL)
        printf("The symbol is an offset into the TLS data area.\n");
    if (flags & SYMFLAG_VALUEPRESENT)
        printf("The Value member is used.\n");
    if (flags & SYMFLAG_VIRTUAL)
        printf("The symbol is a virtual symbol created by the SymAddSymbol function.\n");
}


enum register_t {
    R_X64_RBP = 334,
    R_X64_RSP = 335
    /* TODO: rest */
};


void lgdb_get_symbol_value(struct lgdb_process_ctx *ctx, SYMBOL_INFO *sym_info, lgdb_symbol_t *symbol) {
    if (!sym_info) {
        sym_info = (SYMBOL_INFO *)lgdb_lnmalloc(&ctx->lnmem, sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
        memset(sym_info, 0, sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
        sym_info->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym_info->MaxNameLen = MAX_SYM_NAME;

        WIN32_CALL(SymFromIndex, ctx->proc_info.hProcess, ctx->process_pdb_base, symbol->sym_index, sym_info);
    }

    lgdb_symbol_data_t *data = &symbol->data;

    /* First check what kind of symbol we are dealing with (data, function, type, etc...) */
    DWORD symbol_tag;
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        sym_info->ModBase,
        sym_info->Index,
        TI_GET_SYMTAG,
        &symbol_tag);

    symbol->data_tag = symbol_tag;

    switch (symbol_tag) {

        /* This is data! great */
    case SymTagData: {
        /* Is this of any kind of base type? */
        DWORD type_tag;
        WIN32_CALL(
            SymGetTypeInfo,
            ctx->proc_info.hProcess,
            sym_info->ModBase,
            sym_info->TypeIndex, // <-- We are now checking the tag of the type symbol
            TI_GET_SYMTAG,
            &type_tag);

        switch (type_tag) {

            /* This is a base type! Phew - easy to handle */
        case SymTagBaseType: {
            uint32_t base_type;
            WIN32_CALL(
                SymGetTypeInfo,
                ctx->proc_info.hProcess,
                sym_info->ModBase,
                sym_info->TypeIndex,
                TI_GET_BASETYPE,
                &base_type);

            data->base_type = base_type;
            data->children_count = 0;
            data->size = sym_info->Size;
            data->type_tag = type_tag;

            uint64_t base = 0;

            if (sym_info->Flags & SYMFLAG_REGREL) {
                switch (sym_info->Register) {
                case R_X64_RBP: {
                    base = ctx->thread_ctx.Rbp;
                } break;

                case R_X64_RSP: {
                    base = ctx->thread_ctx.Rsp;
                } break;

                default: {
                    printf("Unknown register!\n");
                    assert(0);
                } break;
                }
            }
            else if (sym_info->Flags & SYMFLAG_REGISTER) {

            }
            else {
                /* Just get the raw address */
            }

            void *addr = (void *)(base + sym_info->Address);

            size_t bytes_read;
            ReadProcessMemory(
                ctx->proc_info.hProcess,
                addr,
                &data->value.bits,
                sizeof(data->value.bits),
                &bytes_read);
        } break;

        }
    } break;

        /* We aren't handling anything else yet */
    default: {
    } break;
 
    }
}


static BOOL s_update_symbols(
    PSYMBOL_INFO pSymInfo,
    ULONG SymbolSize,
    PVOID UserContext) {
    lgdb_process_ctx_t *ctx = (lgdb_process_ctx_t *)UserContext;

    lgdb_entry_value_t *entry = lgdb_get_from_tables(
        &ctx->symbols.sym_name_to_ptr,
        pSymInfo->Name);

    if (entry) {
        /* Simply update the value(s) stored in the symbol */
        lgdb_symbol_t *sym = (lgdb_symbol_t *)(*entry);
        lgdb_get_symbol_value(ctx, pSymInfo, sym);
    }
    else {
        lgdb_symbol_t *new_sym = LGDB_LNMALLOC(&ctx->symbols.data_mem, lgdb_symbol_t, 1);
        new_sym->sym_index = pSymInfo->Index;
        new_sym->type_index = pSymInfo->TypeIndex;

        lgdb_get_symbol_value(ctx, pSymInfo, new_sym);

        /* Register to the table */
        lgdb_insert_in_tables(
            &ctx->symbols.sym_name_to_ptr,
            pSymInfo->Name,
            (uint64_t)new_sym);
    }

#if 0
    // printf("%s %d:\n", pSymInfo->Name, pSymInfo->TypeIndex);

    /* Analyse the C/C++ type of this symbol */
    DWORD tag;
    WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, pSymInfo->ModBase, pSymInfo->TypeIndex, TI_GET_SYMTAG, &tag);

    /* Maximum tags are 42 */
    switch (tag) {
        /* Actual values can only really be of this type */
    case SymTagBaseType: {
        DWORD type; // int, float, etc...
        WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, pSymInfo->ModBase, pSymInfo->TypeIndex, TI_GET_BASETYPE, &type);
        printf("variable of type %d (%s)\n", type, lgdb_get_base_type_string(type));
    } break;

        /* User-defined type */
    case SymTagUDT: {
    } break;

    case SymTagEnum: {
    } break;

    case SymTagFunctionType: {
    } break;

    case SymTagPointerType: {
    } break;

    case SymTagArrayType: {
        DWORD64 length;
        WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, pSymInfo->ModBase, pSymInfo->TypeIndex, TI_GET_LENGTH, &length);

        DWORD type; // int, float, etc...
        WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, pSymInfo->ModBase, pSymInfo->TypeIndex, TI_GET_TYPE, &type);

        DWORD element_type;
        WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, pSymInfo->ModBase, type, TI_GET_BASETYPE, &element_type);

        printf("array of length %llu and element type %d (%s)\n", length, element_type, lgdb_get_base_type_string(element_type));
    } break;

    case SymTagTypedef: {
    } break;

    case SymTagBaseClass: {
    } break;

    case SymTagFunctionArgType: {
    } break;

        /* For now we just handle the above */
    default: {
    } break;

    }
#endif

    return 1;
}


void lgdb_update_local_symbols(struct lgdb_process_ctx *ctx) {
    WIN32_CALL(SymEnumSymbols, ctx->proc_info.hProcess, 0, "*", s_update_symbols, ctx);
}


lgdb_symbol_t *lgdb_get_registered_symbol(struct lgdb_process_ctx *ctx, const char *name) {
    lgdb_entry_value_t *entry = lgdb_get_from_tables(
        &ctx->symbols.sym_name_to_ptr,
        name);

    if (entry) {
        return (lgdb_symbol_t *)(*entry);
    }
    else {
        return NULL;
    }
}
