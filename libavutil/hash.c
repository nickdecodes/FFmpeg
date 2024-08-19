/*
 * Copyright (C) 2013 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hash.h"

#include "adler32.h"
#include "crc.h"
#include "md5.h"
#include "murmur3.h"
#include "ripemd.h"
#include "sha.h"
#include "sha512.h"

#include "avstring.h"
#include "base64.h"
#include "error.h"
#include "intreadwrite.h"
#include "mem.h"

// ENTRY(HASH_TYPE, HASH_NAME, HASH_SIZE)
#define HASHES(ENTRY)                   \
    ENTRY(MD5,        "MD5",        16) \
    ENTRY(MURMUR3,    "murmur3",    16) \
    ENTRY(RIPEMD128,  "RIPEMD128",  16) \
    ENTRY(RIPEMD160,  "RIPEMD160",  20) \
    ENTRY(RIPEMD256,  "RIPEMD256",  32) \
    ENTRY(RIPEMD320,  "RIPEMD320",  40) \
    ENTRY(SHA160,     "SHA160",     20) \
    ENTRY(SHA224,     "SHA224",     28) \
    ENTRY(SHA256,     "SHA256",     32) \
    ENTRY(SHA512_224, "SHA512/224", 28) \
    ENTRY(SHA512_256, "SHA512/256", 32) \
    ENTRY(SHA384,     "SHA384",     48) \
    ENTRY(SHA512,     "SHA512",     64) \
    ENTRY(CRC32,      "CRC32",       4) \
    ENTRY(ADLER32,    "adler32",     4) \

enum hashtype {
#define HASH_TYPE(TYPE, NAME, SIZE) TYPE,
    HASHES(HASH_TYPE)
    NUM_HASHES
};

typedef struct AVHashContext {
    void *ctx;
    enum hashtype type;
    const AVCRC *crctab;
    uint32_t crc;
} AVHashContext;

#define HASH_MAX_SIZE(TYPE, NAME, SIZE) \
    HASH_MAX_SIZE_BEFORE_ ## TYPE,      \
    HASH_MAX_SIZE_UNTIL_ ## TYPE ## _MINUS_ONE = FFMAX(SIZE, HASH_MAX_SIZE_BEFORE_ ## TYPE) - 1,
enum {
    HASHES(HASH_MAX_SIZE)
    MAX_HASH_SIZE
};
static_assert(AV_HASH_MAX_SIZE >= MAX_HASH_SIZE, "AV_HASH_MAX_SIZE needs to be updated!");

#define HASH_MAX_NAME_SIZE(TYPE, NAME, SIZE) \
    HASH_MAX_NAME_SIZE_BEFORE_ ## TYPE,      \
    HASH_MAX_NAME_SIZE_UNTIL_ ## TYPE ## _MINUS_ONE = FFMAX(sizeof(NAME), HASH_MAX_NAME_SIZE_BEFORE_ ## TYPE) - 1,
enum {
    HASHES(HASH_MAX_NAME_SIZE)
    MAX_HASH_NAME_SIZE
};

static const struct {
    const char name[MAX_HASH_NAME_SIZE];
    int size;
} hashdesc[] = {
#define HASH_DESC(TYPE, NAME, SIZE) [TYPE] = { NAME, SIZE },
    HASHES(HASH_DESC)
};

const char *av_hash_names(int i)
{
    if (i < 0 || i >= NUM_HASHES) return NULL;
    return hashdesc[i].name;
}

const char *av_hash_get_name(const AVHashContext *ctx)
{
    return hashdesc[ctx->type].name;
}

int av_hash_get_size(const AVHashContext *ctx)
{
    return hashdesc[ctx->type].size;
}

int av_hash_alloc(AVHashContext **ctx, const char *name)  // 定义一个函数 av_hash_alloc，返回整数，接受一个指向 AVHashContext 指针的指针 ctx 和一个常量字符指针 name
{
    AVHashContext *res;  // 定义一个 AVHashContext 类型的指针 res
    int i;  // 定义一个整数变量 i

    *ctx = NULL;  // 将传入的 ctx 指针指向的内容置为空

    for (i = 0; i < NUM_HASHES; i++)  // 遍历所有可能的哈希类型
        if (av_strcasecmp(name, hashdesc[i].name) == 0)  // 比较输入的名称和当前哈希类型的名称（不区分大小写）
            break;  // 如果匹配，退出循环

    if (i >= NUM_HASHES) return AVERROR(EINVAL);  // 如果没有找到匹配的哈希类型，返回无效参数错误码

    res = av_mallocz(sizeof(*res));  // 分配并清零内存来存储 AVHashContext
    if (!res) return AVERROR(ENOMEM);  // 如果内存分配失败，返回内存不足错误码

    res->type = i;  // 设置哈希类型

    switch (i) {  // 根据哈希类型进行不同的操作
    case MD5:     res->ctx = av_md5_alloc(); break;  // 如果是 MD5 类型，分配相关上下文
    case MURMUR3: res->ctx = av_murmur3_alloc(); break;  // 如果是 MURMUR3 类型，分配相关上下文
    case RIPEMD128:
    case RIPEMD160:
    case RIPEMD256:
    case RIPEMD320: res->ctx = av_ripemd_alloc(); break;  // 对于不同的 RIPEMD 类型，分配相关上下文
    case SHA160:
    case SHA224:
    case SHA256:  res->ctx = av_sha_alloc(); break;  // 对于不同的 SHA 类型，分配相关上下文
    case SHA512_224:
    case SHA512_256:
    case SHA384:
    case SHA512:  res->ctx = av_sha512_alloc(); break;  // 对于不同的 SHA512 变体类型，分配相关上下文
    case CRC32:   res->crctab = av_crc_get_table(AV_CRC_32_IEEE_LE); break;  // 如果是 CRC32 类型，获取相关表
    case ADLER32: break;  // 对于 ADLER32 类型，不进行额外操作
    }

    if (i != ADLER32 && i != CRC32 && !res->ctx) {  // 对于除了 ADLER32 和 CRC32 之外的类型，如果上下文分配失败
        av_free(res);  // 释放已分配的内存
        return AVERROR(ENOMEM);  // 返回内存不足错误码
    }

    *ctx = res;  // 将分配好的 AVHashContext 指针通过 ctx 传出
    return 0;  // 函数成功，返回 0
}

void av_hash_init(AVHashContext *ctx)
{
    switch (ctx->type) {
    case MD5:     av_md5_init(ctx->ctx); break;
    case MURMUR3: av_murmur3_init(ctx->ctx); break;
    case RIPEMD128: av_ripemd_init(ctx->ctx, 128); break;
    case RIPEMD160: av_ripemd_init(ctx->ctx, 160); break;
    case RIPEMD256: av_ripemd_init(ctx->ctx, 256); break;
    case RIPEMD320: av_ripemd_init(ctx->ctx, 320); break;
    case SHA160:  av_sha_init(ctx->ctx, 160); break;
    case SHA224:  av_sha_init(ctx->ctx, 224); break;
    case SHA256:  av_sha_init(ctx->ctx, 256); break;
    case SHA512_224:  av_sha512_init(ctx->ctx, 224); break;
    case SHA512_256:  av_sha512_init(ctx->ctx, 256); break;
    case SHA384:  av_sha512_init(ctx->ctx, 384); break;
    case SHA512:  av_sha512_init(ctx->ctx, 512); break;
    case CRC32:   ctx->crc = UINT32_MAX; break;
    case ADLER32: ctx->crc = 1; break;
    }
}

void av_hash_update(AVHashContext *ctx, const uint8_t *src, size_t len)
{
    switch (ctx->type) {
    case MD5:     av_md5_update(ctx->ctx, src, len); break;
    case MURMUR3: av_murmur3_update(ctx->ctx, src, len); break;
    case RIPEMD128:
    case RIPEMD160:
    case RIPEMD256:
    case RIPEMD320: av_ripemd_update(ctx->ctx, src, len); break;
    case SHA160:
    case SHA224:
    case SHA256:  av_sha_update(ctx->ctx, src, len); break;
    case SHA512_224:
    case SHA512_256:
    case SHA384:
    case SHA512:  av_sha512_update(ctx->ctx, src, len); break;
    case CRC32:   ctx->crc = av_crc(ctx->crctab, ctx->crc, src, len); break;
    case ADLER32: ctx->crc = av_adler32_update(ctx->crc, src, len); break;
    }
}

void av_hash_final(AVHashContext *ctx, uint8_t *dst)
{
    switch (ctx->type) {
    case MD5:     av_md5_final(ctx->ctx, dst); break;
    case MURMUR3: av_murmur3_final(ctx->ctx, dst); break;
    case RIPEMD128:
    case RIPEMD160:
    case RIPEMD256:
    case RIPEMD320: av_ripemd_final(ctx->ctx, dst); break;
    case SHA160:
    case SHA224:
    case SHA256:  av_sha_final(ctx->ctx, dst); break;
    case SHA512_224:
    case SHA512_256:
    case SHA384:
    case SHA512:  av_sha512_final(ctx->ctx, dst); break;
    case CRC32:   AV_WB32(dst, ctx->crc ^ UINT32_MAX); break;
    case ADLER32: AV_WB32(dst, ctx->crc); break;
    }
}

void av_hash_final_bin(struct AVHashContext *ctx, uint8_t *dst, int size)
{
    uint8_t buf[AV_HASH_MAX_SIZE];
    unsigned rsize = av_hash_get_size(ctx);

    av_hash_final(ctx, buf);
    memcpy(dst, buf, FFMIN(size, rsize));
    if (size > rsize)
        memset(dst + rsize, 0, size - rsize);
}

void av_hash_final_hex(struct AVHashContext *ctx, uint8_t *dst, int size)
{
    uint8_t buf[AV_HASH_MAX_SIZE];
    unsigned rsize = av_hash_get_size(ctx), i;

    av_hash_final(ctx, buf);
    for (i = 0; i < FFMIN(rsize, size / 2); i++)
        snprintf(dst + i * 2, size - i * 2, "%02x", buf[i]);
}

void av_hash_final_b64(struct AVHashContext *ctx, uint8_t *dst, int size)
{
    uint8_t buf[AV_HASH_MAX_SIZE], b64[AV_BASE64_SIZE(AV_HASH_MAX_SIZE)];
    unsigned rsize = av_hash_get_size(ctx), osize;

    av_hash_final(ctx, buf);
    av_base64_encode(b64, sizeof(b64), buf, rsize);
    osize = AV_BASE64_SIZE(rsize);
    memcpy(dst, b64, FFMIN(osize, size));
    if (size < osize)
        dst[size - 1] = 0;
}

void av_hash_freep(AVHashContext **ctx)
{
    if (*ctx)
        av_freep(&(*ctx)->ctx);
    av_freep(ctx);
}
