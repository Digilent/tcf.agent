/*******************************************************************************
 * Copyright (c) 2007, 2010 Wind River Systems, Inc. and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 * The Eclipse Public License is available at
 * http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 * You may elect to redistribute this code under either of these licenses.
 *
 * Contributors:
 *     Wind River Systems - initial API and implementation
 *******************************************************************************/

/*
 * This module contains definitions of target CPU registers and stack frames.
 */

#include <tcf/config.h>

#if ENABLE_DebugContext

#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <tcf/framework/cpudefs.h>
#include <tcf/framework/errors.h>
#include <tcf/framework/context.h>
#include <tcf/framework/myalloc.h>
#include <tcf/framework/exceptions.h>
#include <tcf/services/symbols.h>

#include <tcf/framework/cpudefs-ext.h>

int read_reg_value(StackFrame * frame, RegisterDefinition * reg_def, uint64_t * value) {
    uint8_t buf[8];
    if (reg_def == NULL) {
        set_errno(ERR_INV_CONTEXT, "Invalid register");
        return -1;
    }
    if (frame == NULL) {
        set_errno(ERR_INV_CONTEXT, "Invalid stack frame");
        return -1;
    }
    if (reg_def->size > sizeof(buf)) {
        errno = ERR_INV_DATA_SIZE;
        return -1;
    }
    if (read_reg_bytes(frame, reg_def, 0, reg_def->size, buf) < 0) return -1;
    if (value != NULL) {
        size_t i;
        uint64_t n = 0;
        for (i = 0; i < reg_def->size; i++) {
            n = n << 8;
            n |= buf[reg_def->big_endian ? i : reg_def->size - i - 1];
        }
        *value = n;
    }
    return 0;
}

int write_reg_value(StackFrame * frame, RegisterDefinition * reg_def, uint64_t value) {
    size_t i;
    uint8_t buf[8];
    if (reg_def == NULL) {
        set_errno(ERR_INV_CONTEXT, "Invalid register");
        return -1;
    }
    if (frame == NULL) {
        set_errno(ERR_INV_CONTEXT, "Invalid stack frame");
        return -1;
    }
    if (reg_def->size > sizeof(buf)) {
        errno = ERR_INV_DATA_SIZE;
        return -1;
    }
    for (i = 0; i < reg_def->size; i++) {
        buf[reg_def->big_endian ? reg_def->size - i - 1 : i] = (uint8_t)value;
        value = value >> 8;
    }
    if (write_reg_bytes(frame, reg_def, 0, reg_def->size, buf) < 0) return -1;
    if (!frame->is_top_frame) frame->has_reg_data = 1;
    return 0;
}

ContextAddress get_regs_PC(Context * ctx) {
    size_t i;
    uint8_t buf[8];
    ContextAddress pc = 0;
    RegisterDefinition * def = get_PC_definition(ctx);
    if (def == NULL) return 0;
    assert(def->size <= sizeof(buf));
    if (context_read_reg(ctx, def, 0, def->size, buf) < 0) return 0;
    for (i = 0; i < def->size; i++) {
        pc = pc << 8;
        pc |= buf[def->big_endian ? i : def->size - i - 1];
    }
    return pc;
}

void set_regs_PC(Context * ctx, ContextAddress pc) {
    size_t i;
    uint8_t buf[8];
    RegisterDefinition * def = get_PC_definition(ctx);
    if (def == NULL) return;
    assert(def->size <= sizeof(buf));
    for (i = 0; i < def->size; i++) {
        buf[def->big_endian ? def->size - i - 1 : i] = (uint8_t)pc;
        pc = pc >> 8;
    }
    context_write_reg(ctx, def, 0, def->size, buf);
}

int id2frame(const char * id, Context ** ctx, int * frame) {
    int f = 0;
    Context * c = NULL;

    if (*id++ != 'F') {
        errno = ERR_INV_CONTEXT;
        return -1;
    }
    if (*id++ != 'P') {
        errno = ERR_INV_CONTEXT;
        return -1;
    }
    while (*id != '.') {
        if (*id < '0' || *id > '9') {
            errno = ERR_INV_CONTEXT;
            return -1;
        }
        f = f * 10 + (*id++ - '0');
    }
    id++;
    c = id2ctx(id);
    if (c == NULL) {
        errno = ERR_INV_CONTEXT;
        return -1;
    }
    *ctx = c;
    *frame = f;
    return 0;
}

const char * frame2id(Context * ctx, int frame) {
    static char id[256];

    assert(frame >= 0);
    if (!context_has_state(ctx)) {
        errno = ERR_INV_CONTEXT;
        return NULL;
    }
    snprintf(id, sizeof(id), "FP%d.%s", frame, ctx->id);
    return id;
}

const char * register2id(Context * ctx, int frame, RegisterDefinition * reg) {
    static char id[256];
    RegisterDefinition * defs = get_reg_definitions(ctx);
    if (frame < 0) {
        snprintf(id, sizeof(id), "R%d.%s", (int)(reg - defs), ctx->id);
    }
    else {
        snprintf(id, sizeof(id), "R%d@%d.%s", (int)(reg - defs), frame, ctx->id);
    }
    return id;
}

int id2register(const char * id, Context ** ctx, int * frame, RegisterDefinition ** reg_def) {
    int r = 0;

    *ctx = NULL;
    *frame = STACK_TOP_FRAME;
    *reg_def = NULL;
    if (*id++ != 'R') {
        errno = ERR_INV_CONTEXT;
        return -1;
    }
    while (*id != '.' && *id != '@') {
        if (*id >= '0' && *id <= '9') {
            r = r * 10 + (*id++ - '0');
        }
        else {
            errno = ERR_INV_CONTEXT;
            return -1;
        }
    }
    if (*id == '@') {
        int n = 0;
        id++;
        while (*id != '.') {
            if (*id >= '0' && *id <= '9') {
                n = n * 10 + (*id++ - '0');
            }
            else {
                errno = ERR_INV_CONTEXT;
                return -1;
            }
        }
        *frame = n;
    }
    id++;
    *ctx = id2ctx(id);
    if (*ctx == NULL) {
        errno = ERR_INV_CONTEXT;
        return -1;
    }
    if ((*ctx)->exited) {
        errno = ERR_ALREADY_EXITED;
        return -1;
    }
    *reg_def = get_reg_definitions(*ctx) + r;
    return 0;
}

static void location_expression_error(void) {
    str_exception(ERR_OTHER, "Invalid location expression");
}

LocationExpressionState * evaluate_location_expression(Context * ctx, StackFrame * frame,
                                     LocationExpressionCommand * cmds, unsigned cmd_cnt,
                                     uint64_t * args, unsigned args_cnt) {
    unsigned i;
    unsigned stk_pos = 0;
    unsigned stk_max = 0;
    uint64_t * stk = NULL;
    LocationExpressionState * state = (LocationExpressionState *)tmp_alloc_zero(sizeof(LocationExpressionState));

    state->ctx = ctx;
    state->stack_frame = frame;
    state->args = args;
    state->args_cnt = args_cnt;
    for (i = 0; i < cmd_cnt; i++) {
        LocationExpressionCommand * cmd = cmds + i;
        if (stk_pos >= stk_max) {
            stk_max += 4;
            stk = (uint64_t *)tmp_realloc(stk, sizeof(uint64_t) * stk_max);
        }
        switch (cmd->cmd) {
        case SFT_CMD_NUMBER:
            stk[stk_pos++] = cmd->args.num;
            break;
        case SFT_CMD_REGISTER:
            if (read_reg_value(frame, cmd->args.reg, stk + stk_pos) < 0) exception(errno);
            stk_pos++;
            break;
        case SFT_CMD_FP:
            stk[stk_pos++] = frame->fp;
            break;
        case SFT_CMD_DEREF:
            if (stk_pos < 1) location_expression_error();
            {
                size_t j;
                size_t size = cmd->args.deref.size;
                uint64_t n = 0;
                uint8_t buf[8];

                if (context_read_mem(ctx, (ContextAddress)stk[stk_pos - 1], buf, size) < 0) exception(errno);
                for (j = 0; j < size; j++) {
                    n = (n << 8) | buf[cmd->args.deref.big_endian ? j : size - j - 1];
                }
                stk[stk_pos - 1] = n;
            }
            break;
        case SFT_CMD_ADD:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] + stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_SUB:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] - stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_MUL:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] * stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_DIV:
            if (stk_pos < 2) location_expression_error();
            if (stk[stk_pos - 1] == 0) str_exception(ERR_OTHER, "Division by zero in location expression");
            stk[stk_pos - 2] = stk[stk_pos - 2] / stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_AND:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] & stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_OR:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] | stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_XOR:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] ^ stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_NEG:
            if (stk_pos < 1) location_expression_error();
            stk[stk_pos - 1] = ~stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_GE:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] >= stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_GT:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] > stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_LE:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] <= stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_LT:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] = stk[stk_pos - 2] < stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_SHL:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] <<= stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_SHR:
            if (stk_pos < 2) location_expression_error();
            stk[stk_pos - 2] >>= stk[stk_pos - 1];
            stk_pos--;
            break;
        case SFT_CMD_ARG:
            if (cmd->args.arg_no >= args_cnt) location_expression_error();
            stk[stk_pos++] = args[cmd->args.arg_no];
            break;
        case SFT_CMD_LOCATION:
            state->stk = stk;
            state->stk_pos = stk_pos;
            state->stk_max = stk_max;
            state->reg_id_scope = cmd->args.loc.reg_id_scope;
            state->code = cmd->args.loc.code_addr;
            state->code_len = cmd->args.loc.code_size;
            state->code_pos = 0;
            state->addr_size = cmd->args.loc.addr_size;
            state->big_endian = cmd->args.loc.big_endian;
            state->client_op = NULL;
            if (cmd->args.loc.func(state) < 0) exception(errno);
            stk_max = state->stk_max;
            stk_pos = state->stk_pos;
            stk = state->stk;
            break;
        default:
            location_expression_error();
            break;
        }
    }
    state->stk = stk;
    state->stk_pos = stk_pos;
    state->stk_max = stk_max;
    return state;
}

#endif /* ENABLE_DebugContext */