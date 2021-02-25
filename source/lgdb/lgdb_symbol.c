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


static uint32_t s_register_base_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type);
static uint32_t s_register_typedef_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type);
static uint32_t s_register_pointer_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type);
static uint32_t s_register_array_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type);
static uint32_t s_register_udt_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type);
static uint32_t s_register_function_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type);
static uint32_t s_register_enum_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type);


lgdb_symbol_type_t *lgdb_get_type(struct lgdb_process_ctx *ctx, uint32_t type_index) {
    lgdb_entry_value_t *entry = lgdb_get_from_table(&ctx->symbols.type_idx_to_ptr, type_index);

    if (!entry) {
        /* We need to register */
        lgdb_symbol_type_t *new_type = LGDB_LNMALLOC(&ctx->symbols.type_mem, lgdb_symbol_type_t, 1);
        lgdb_insert_in_table(&ctx->symbols.type_idx_to_ptr, type_index, (lgdb_entry_value_t)new_type);

        new_type->index = type_index;

        /* Query some basic information, then get into meat of this type */
        uint32_t tag;
        WIN32_CALL(
            SymGetTypeInfo,
            ctx->proc_info.hProcess,
            ctx->process_pdb_base,
            new_type->index,
            TI_GET_SYMTAG,
            &tag);
        new_type->tag = tag;

        ULONG64 length;
        WIN32_CALL(
            SymGetTypeInfo,
            ctx->proc_info.hProcess,
            ctx->process_pdb_base,
            new_type->index,
            TI_GET_LENGTH,
            &length);
        new_type->size = (uint32_t)length;

        switch (tag) {
        case SymTagBaseType: s_register_base_type(ctx, new_type); break;
        case SymTagTypedef: s_register_typedef_type(ctx, new_type); break;
        case SymTagPointerType: s_register_pointer_type(ctx, new_type); break;
        case SymTagArrayType: s_register_array_type(ctx, new_type); break;
        case SymTagUDT: s_register_udt_type(ctx, new_type); break;
        case SymTagFunctionType: s_register_function_type(ctx, new_type); break;
        case SymTagEnum: s_register_enum_type(ctx, new_type); break;
        default: {
            printf("Unrecognised type!\n");
            assert(0);
            return NULL;
        } break;
        }

        return new_type;
    }
    else {
        lgdb_symbol_type_t *type = (lgdb_symbol_type_t *)(*entry);
        return type;
    }
}


static uint32_t s_register_base_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type) {
    uint32_t base_type;
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type->index,
        TI_GET_BASETYPE,
        &base_type);
    type->uinfo.base_type.base_type = base_type;

    return type->index;
}


static uint32_t s_register_typedef_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type) {
    uint32_t aliased_type;
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type->index,
        TI_GET_TYPE,
        &aliased_type);
    type->uinfo.typedef_type.type_index = aliased_type;

    return type->index;
}


static uint32_t s_register_pointer_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type) {
    uint32_t pointed_type;
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type->index,
        TI_GET_TYPE,
        &pointed_type);
    type->uinfo.pointer_type.type_index = pointed_type;

    return type->index;
}


static uint32_t s_register_array_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type) {
    uint32_t arrayed_type;
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type->index,
        TI_GET_TYPE,
        &arrayed_type);
    type->uinfo.array_type.type_index = arrayed_type;

    return type->index;
}


static TI_FINDCHILDREN_PARAMS *s_find_children(lgdb_process_ctx_t *ctx, uint32_t type_idx, uint32_t *children_count) {
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type_idx,
        TI_GET_CHILDRENCOUNT,
        children_count);

    uint32_t buf_size = sizeof(TI_FINDCHILDREN_PARAMS) + *children_count * sizeof(ULONG);
    TI_FINDCHILDREN_PARAMS *children_params = (TI_FINDCHILDREN_PARAMS *)lgdb_lnmalloc(&ctx->lnmem, buf_size);
    memset(children_params, 0, buf_size);

    children_params->Count = *children_count;

    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type_idx,
        TI_FINDCHILDREN,
        children_params);

    return children_params;
}


static uint32_t s_register_udt_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type) {
    uint32_t children_count;
    TI_FINDCHILDREN_PARAMS *children_params = s_find_children(ctx, type->index, &children_count);

    type->uinfo.udt_type.children_count = children_count;

    /* Temporary buffers */
    uint32_t member_vars_count = 0;
    lgdb_member_var_t *member_vars = LGDB_LNMALLOC(&ctx->lnmem, lgdb_member_var_t, TIS_MAXNUMCHILDREN);

    uint32_t methods_count = 0;
    uint32_t *methods= LGDB_LNMALLOC(&ctx->lnmem, uint32_t, TIS_MAXNUMCHILDREN);

    uint32_t base_classes_count = 0;
    lgdb_base_class_t *base_classes = LGDB_LNMALLOC(&ctx->lnmem, lgdb_base_class_t, TIS_MAXNUMCHILDREN);

    for (uint32_t i = 0; i < children_count; ++i) {
        uint32_t tag;
        WIN32_CALL(
            SymGetTypeInfo,
            ctx->proc_info.hProcess,
            ctx->process_pdb_base,
            children_params->ChildId[i],
            TI_GET_SYMTAG,
            &tag);

        if (tag == SymTagData) {
            /* This is a member variable */
            uint32_t offset;
            WIN32_CALL(
                SymGetTypeInfo,
                ctx->proc_info.hProcess,
                ctx->process_pdb_base,
                children_params->ChildId[i],
                TI_GET_OFFSET,
                &offset);

            uint32_t type_idx;
            WIN32_CALL(
                SymGetTypeInfo,
                ctx->proc_info.hProcess,
                ctx->process_pdb_base,
                children_params->ChildId[i],
                TI_GET_TYPE,
                &type_idx);

            DWORD64 length;
            WIN32_CALL(
                SymGetTypeInfo,
                ctx->proc_info.hProcess,
                ctx->process_pdb_base,
                type_idx,
                TI_GET_LENGTH,
                &length);

            member_vars[member_vars_count].offset = offset;
            member_vars[member_vars_count].size = (uint32_t)length;
            member_vars[member_vars_count].sym_idx = children_params->ChildId[i];
            member_vars[member_vars_count++].type_idx = type_idx;
        }
        else if (tag == SymTagFunction) {
            /* This is a method */
            methods[methods_count++] = children_params->ChildId[i];
        }
        else if (tag == SymTagBaseClass) {
            uint32_t offset;
            WIN32_CALL(
                SymGetTypeInfo,
                ctx->proc_info.hProcess,
                ctx->process_pdb_base,
                children_params->ChildId[i],
                TI_GET_OFFSET,
                &offset);

            uint32_t type_idx;
            WIN32_CALL(
                SymGetTypeInfo,
                ctx->proc_info.hProcess,
                ctx->process_pdb_base,
                children_params->ChildId[i],
                TI_GET_TYPE,
                &type_idx);

            DWORD64 length;
            WIN32_CALL(
                SymGetTypeInfo,
                ctx->proc_info.hProcess,
                ctx->process_pdb_base,
                type_idx,
                TI_GET_LENGTH,
                &length);

            /* This is a base class */
            base_classes[base_classes_count].offset = offset;
            base_classes[base_classes_count].type_idx = type_idx;
            base_classes[base_classes_count].size = (uint32_t)length;
            base_classes[base_classes_count++].idx = children_params->ChildId[i];
        }
    }

    uint8_t *children_storage = (uint8_t *)lgdb_lnmalloc(
        &ctx->symbols.type_mem,
            sizeof(lgdb_member_var_t) * member_vars_count +
            sizeof(uint32_t) * methods_count +
            sizeof(lgdb_base_class_t) * base_classes_count);

    uint32_t member_vars_size = sizeof(lgdb_member_var_t) * member_vars_count;
    uint32_t methods_size = sizeof(uint32_t) * methods_count;
    uint32_t base_classes_size = sizeof(lgdb_base_class_t) * base_classes_count;

    memcpy(children_storage, member_vars, member_vars_size);
    uint32_t offset = member_vars_size;
    memcpy(children_storage + offset, methods, methods_size);
    offset += methods_size;
    memcpy(children_storage + offset, base_classes, base_classes_size);

    type->uinfo.udt_type.member_var_count = member_vars_count;
    type->uinfo.udt_type.member_vars_type_idx = (lgdb_member_var_t *)children_storage;

    type->uinfo.udt_type.methods_count = methods_count;
    type->uinfo.udt_type.methods_type_idx = (uint32_t *)(children_storage + member_vars_size);

    type->uinfo.udt_type.base_classes_count = base_classes_count;
    type->uinfo.udt_type.base_classes_type_idx = (lgdb_base_class_t *)(children_storage + member_vars_size + methods_size);

    return type->index;
}


static uint32_t s_register_function_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type) {
    uint32_t return_type_index;
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type->index,
        TI_GET_TYPEID,
        &return_type_index);
    type->uinfo.function_type.return_type_index = return_type_index;

    uint32_t arg_count;
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type->index,
        TI_GET_COUNT,
        &arg_count);
    type->uinfo.function_type.arg_count = arg_count;

    uint32_t class_id;
    BOOL result = SymGetTypeInfo(
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type->index,
        TI_GET_CLASSPARENTID,
        &class_id);

    if (result) {
        type->uinfo.function_type.class_id = class_id;

        uint32_t this_adjust;
        WIN32_CALL(
            SymGetTypeInfo,
            ctx->proc_info.hProcess,
            ctx->process_pdb_base,
            type->index,
            TI_GET_THISADJUST,
            &this_adjust);
        type->uinfo.function_type.this_adjust = this_adjust;
    }
    else {
        type->uinfo.function_type.class_id = 0xFFFFFFFF;
    }

    /* Get function arguments */
    uint32_t children_count;
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type->index,
        TI_GET_CHILDRENCOUNT,
        &children_count);

    if (children_count) {
        uint32_t children_count;
        TI_FINDCHILDREN_PARAMS *children_params = s_find_children(ctx, type->index, &children_count);

        uint32_t counter = 0;
        uint32_t *tmp_args = LGDB_LNMALLOC(&ctx->lnmem, uint32_t, children_count);

        for (uint32_t i = 0; i < children_count && counter < arg_count; ++i) {
            uint32_t tag;
            WIN32_CALL(
                SymGetTypeInfo,
                ctx->proc_info.hProcess,
                ctx->process_pdb_base,
                children_params->ChildId[i],
                TI_GET_SYMTAG,
                &tag);

            uint32_t type;
            WIN32_CALL(
                SymGetTypeInfo,
                ctx->proc_info.hProcess,
                ctx->process_pdb_base,
                children_params->ChildId[i],
                TI_GET_TYPE,
                &type);

            if (tag == SymTagFunctionArgType) {
                tmp_args[counter++] = type;
            }
        }

        assert(counter == arg_count);

        type->uinfo.function_type.arg_types = LGDB_LNMALLOC(
            &ctx->symbols.type_mem,
            uint32_t,
            counter);

        memcpy(type->uinfo.function_type.arg_types, tmp_args, sizeof(uint32_t) * counter);

        return type->index;
    }
    else {
        type->uinfo.function_type.arg_count = 0;
        return type->index;
    }
}


static uint32_t s_register_enum_type(lgdb_process_ctx_t *ctx, lgdb_symbol_type_t *type) {
    uint32_t type_index;
    WIN32_CALL(
        SymGetTypeInfo,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        type->index,
        TI_GET_TYPEID,
        &type_index);
    type->uinfo.enum_type.type_index = type_index;
    

    uint32_t children_count;
    TI_FINDCHILDREN_PARAMS *children_params = s_find_children(ctx, type->index, &children_count);

    uint32_t enum_count = 0;

    /* Allocate a table which associates a value to an enumerator value */
    type->uinfo.enum_type.value_to_name = lgdb_create_table(
        children_count, children_count,
        LGDB_LNMALLOC(&ctx->symbols.type_mem, lgdb_handle_t, children_count),
        LGDB_LNMALLOC(&ctx->symbols.type_mem, lgdb_entry_t, children_count));

    for (uint32_t i = 0; i < children_count; ++i) {
        uint32_t tag;
        WIN32_CALL(
            SymGetTypeInfo,
            ctx->proc_info.hProcess,
            ctx->process_pdb_base,
            children_params->ChildId[i],
            TI_GET_SYMTAG,
            &tag);

        if (tag == SymTagData) {
            ++enum_count;
        }

        wchar_t *src_name;
        WIN32_CALL(SymGetTypeInfo,
            ctx->proc_info.hProcess,
            ctx->process_pdb_base,
            children_params->ChildId[i],
            TI_GET_SYMNAME,
            &src_name);

        uint32_t length = (uint32_t)wcslen(src_name);
        wchar_t *dst_name = LGDB_LNMALLOC(&ctx->symbols.type_mem, wchar_t, length + 1);
        wcsncpy_s(dst_name, length, src_name, length);
        dst_name[length] = 0;

        VARIANT variant;
        WIN32_CALL(
            SymGetTypeInfo,
            ctx->proc_info.hProcess,
            ctx->process_pdb_base,
            children_params->ChildId[i],
            TI_GET_VALUE,
            &variant);

        lgdb_insert_in_table(&type->uinfo.enum_type.value_to_name, variant.intVal, (lgdb_entry_value_t)dst_name);

        /* Needs to be freed */
        LocalFree(src_name);
    }

    return type->index;
}


static void *s_get_real_sym_address(lgdb_process_ctx_t *ctx, PSYMBOL_INFO pSymInfo, lgdb_symbol_t *sym) {
    uint64_t base;
    if (pSymInfo->Flags & SYMFLAG_REGREL) {
        switch (pSymInfo->Register) {
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
    else if (pSymInfo->Flags & SYMFLAG_REGISTER) {
        /* Need to handle this when the time comes */
        assert(0);
    }
    else {
        /* Just get the raw address */
    }

    void *addr = (void *)(base + sym->start_addr);
    return addr;
}


static BOOL s_update_symbols_depr(
    PSYMBOL_INFO pSymInfo,
    ULONG SymbolSize,
    PVOID UserContext) {
    lgdb_process_ctx_t *ctx = (lgdb_process_ctx_t *)UserContext;

    /* First register the type if it hasn't been yet */
    lgdb_symbol_type_t *type = lgdb_get_type(ctx, pSymInfo->TypeIndex);

    lgdb_entry_value_t *entry = lgdb_get_from_tables(
        &ctx->symbols.sym_name_to_ptr,
        pSymInfo->Name);

    if (entry) {
        /* Simply update the value(s) stored in the symbol */
        lgdb_symbol_t *sym = (lgdb_symbol_t *)(*entry);

        size_t bytes_read;
        WIN32_CALL(ReadProcessMemory,
            ctx->proc_info.hProcess,
            s_get_real_sym_address(ctx, pSymInfo, sym),
            sym->debugger_bytes_ptr,
            sym->size,
            &bytes_read);
    }
    else {
        lgdb_symbol_t *new_sym = LGDB_LNMALLOC(&ctx->symbols.data_mem, lgdb_symbol_t, 1);
        ctx->symbols.symbol_ptr_pool[ctx->symbols.data_symbol_count++] = new_sym;

        new_sym->sym_index = pSymInfo->Index;
        new_sym->type_index = pSymInfo->TypeIndex;
        new_sym->start_addr = pSymInfo->Address;
        new_sym->size = pSymInfo->Size;
        new_sym->sym_tag = pSymInfo->Tag;
        new_sym->debugger_bytes_ptr = LGDB_LNMALLOC(&ctx->symbols.copy_mem, uint8_t, new_sym->size);

        size_t bytes_read;
        WIN32_CALL(ReadProcessMemory,
            ctx->proc_info.hProcess,
            s_get_real_sym_address(ctx, pSymInfo, new_sym),
            new_sym->debugger_bytes_ptr,
            new_sym->size,
            &bytes_read);

        /* Register to the table */
        lgdb_insert_in_tables(
            &ctx->symbols.sym_name_to_ptr,
            pSymInfo->Name,
            (uint64_t)new_sym);
    }

    return 1;
}


static BOOL s_update_symbols(
    PSYMBOL_INFO pSymInfo,
    ULONG SymbolSize,
    PVOID UserContext) {
    lgdb_process_ctx_t *ctx = (lgdb_process_ctx_t *)UserContext;

    lgdb_symbol_t new_sym = {
        .sym_index = pSymInfo->Index,
        .type_index = pSymInfo->TypeIndex,
        .start_addr = pSymInfo->Address,
        .size = pSymInfo->Size,
        .sym_tag = pSymInfo->Tag,
        .debugger_bytes_ptr = LGDB_LNMALLOC(&ctx->lnmem, uint8_t, pSymInfo->Size)
    };

    size_t bytes_read;
    WIN32_CALL(ReadProcessMemory,
        ctx->proc_info.hProcess,
        s_get_real_sym_address(ctx, pSymInfo, &new_sym),
        new_sym.debugger_bytes_ptr,
        new_sym.size,
        &bytes_read);

    assert(ctx->symbols.current_updt_sym_proc);
    ctx->symbols.current_updt_sym_proc(ctx, pSymInfo->Name, &new_sym);

    return 1;
}



void lgdb_update_local_symbols_depr(struct lgdb_process_ctx *ctx) {
    WIN32_CALL(SymEnumSymbols, ctx->proc_info.hProcess, 0, "*", s_update_symbols_depr, ctx);
}


void lgdb_update_local_symbols(struct lgdb_process_ctx *ctx, lgdb_update_symbol_proc_t proc) {
    ctx->symbols.current_updt_sym_proc = proc;
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


static int s_print_chars(char *address, uint32_t count, lgdb_symbol_type_t *type) {
    printf("\'%c\'", address[0]);
    for (uint32_t i = 1; i < count; ++i)
        printf(", \'%c\'", address[i]);

    return 1;
}


static int s_print_ints(int *address, uint32_t count, lgdb_symbol_type_t *type) {
    printf("%d", address[0]);
    for (uint32_t i = 1; i < count; ++i)
        printf(", %d", address[i]);

    return 1;
}


static int s_print_uints(unsigned int *address, uint32_t count, lgdb_symbol_type_t *type) {
    printf("%u", address[0]);
    for (uint32_t i = 1; i < count; ++i)
        printf(", %u", address[i]);

    return 1;
}


static int s_print_floats(float *address, uint32_t count, lgdb_symbol_type_t *type) {
    printf("%f", address[0]);
    for (uint32_t i = 1; i < count; ++i)
        printf(", %f", address[i]);

    return 1;
}


static int s_print_bools(uint8_t *address, uint32_t count, lgdb_symbol_type_t *type) {
    printf("%s", address[0] ? "true" : "false");
    for (uint32_t i = 1; i < count; ++i)
        printf(", %s", address[i] ? "true" : "false");

    return 1;
}


static int s_print_base_type(void *address, uint32_t size, lgdb_symbol_type_t *type) {
    switch (type->uinfo.base_type.base_type) {
    case btVoid: return printf("");
    case btChar: return s_print_chars((char *)address, size / type->size, type);
        // case btWChar:
    case btInt: return s_print_ints((int *)address, size / type->size, type);
    case btUInt: return s_print_uints((unsigned int *)address, size / type->size, type);
    case btFloat: return s_print_floats((float *)address, size / type->size, type);
    case btBCD: return printf("");
    case btBool: return s_print_bools((uint8_t *)address, size / type->size, type);
    case btLong: return s_print_ints((int *)address, size / type->size, type);
    case btULong: return s_print_uints((unsigned int *)address, size / type->size, type);
    case btCurrency: return printf("");
    case btDate: return printf("");
    case btVariant: return printf("");
    case btComplex: return printf("");
    case btBit: return printf("");
    case btBSTR: return printf("");
    case btHresult: return printf(""); //??
    default: return 0;
    }
}


static int s_print_pointer_type(void *address, uint32_t size, lgdb_symbol_type_t *type) {
    return printf("%p", *((void **)address));
}


static int s_print_enum_type(void *address, uint32_t size, lgdb_symbol_type_t *type) {
    int32_t bytes;

    switch (size) {
    case 1: bytes = *(int8_t *)address; break;
    case 2: bytes = *(int16_t *)address; break;
    case 4: bytes = *(int32_t *)address; break;
    default: {
        printf("Unknown enum size\n");
        assert(0);
    } break;
    }

    lgdb_entry_value_t *entry = lgdb_get_from_table(&type->uinfo.enum_type.value_to_name, bytes);

    if (entry) {
        wchar_t *enum_value_name = (wchar_t *)*entry;
        return wprintf(L"%s (%d)", enum_value_name, bytes);
    }
    else {
        return printf("Unknown enum value (%d)", bytes);
    }
}


static int s_print_data(lgdb_process_ctx_t *ctx, void *address, uint32_t size, lgdb_symbol_type_t *type) {
    switch (type->tag) {
    case SymTagBaseType: return s_print_base_type(address, size, type);

    case SymTagTypedef: {
        lgdb_symbol_type_t *typedefed_type = lgdb_get_type(ctx, type->uinfo.typedef_type.type_index);

        return s_print_data(ctx, address, size, typedefed_type);
    };

    case SymTagPointerType: {
        return s_print_pointer_type(address, size, type);
    };

    case SymTagArrayType: {
        lgdb_symbol_type_t *arrayed_type = lgdb_get_type(ctx, type->uinfo.array_type.type_index);

        return s_print_data(ctx, address, size, arrayed_type);
    };

    case SymTagUDT: {
        /* Print the base classes members */
        for (uint32_t i = 0; i < type->uinfo.udt_type.base_classes_count; ++i) {
            lgdb_base_class_t *base_class = &type->uinfo.udt_type.base_classes_type_idx[i];

            lgdb_symbol_type_t *base_class_type = lgdb_get_type(ctx, base_class->type_idx);

            s_print_data(ctx, (uint8_t *)address + base_class->offset, base_class->size, base_class_type);

            putchar('\n');
        }
        
        /* Print the member variables */
        for (uint32_t i = 0; i < type->uinfo.udt_type.member_var_count; ++i) {
            lgdb_member_var_t *var = &type->uinfo.udt_type.member_vars_type_idx[i];

            lgdb_symbol_type_t *member_type = lgdb_get_type(ctx, var->type_idx);

            s_print_data(ctx, (uint8_t *)address + var->offset, var->size, member_type);

            putchar('\n');
        }

        return 1;
    };

    case SymTagEnum: {
        // lgdb_symbol_type_t *enum_type = lgdb_get_type(ctx, type->uinfo.enum_type.type_index);
        return s_print_enum_type(address, size, type);
    };

    default: return 0;
    }
}


void lgdb_print_symbol_value(struct lgdb_process_ctx *ctx, const char *name) {
    lgdb_entry_value_t *entry = lgdb_get_from_tables(
        &ctx->symbols.sym_name_to_ptr,
        name);

    if (entry) {
        lgdb_symbol_t *sym = (lgdb_symbol_t *)(*entry);

        /* TODO: Use lgdb_get_type */
        lgdb_entry_value_t *type_entry = lgdb_get_from_table(&ctx->symbols.type_idx_to_ptr, sym->type_index);
        lgdb_symbol_type_t *type = (lgdb_symbol_type_t *)(*type_entry);

        /* Simply update the value(s) stored in the symbol */
        s_print_data(ctx, sym->debugger_bytes_ptr, sym->size, type);
    }
    else {
        
    }
}

