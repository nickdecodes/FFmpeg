/*
 * AES-CTR cipher
 * Copyright (c) 2015 Eran Kornblau <erankor at gmail dot com>
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
/*
 * AES-CTR 加密算法
 * 版权声明等信息
 */

#include <string.h>
// 包含标准字符串处理库

#include "aes_ctr.h"
#include "aes.h"
#include "aes_internal.h"
#include "intreadwrite.h"
#include "macros.h"
#include "mem.h"
#include "random_seed.h"
// 包含相关的头文件

// 定义 AES 块的大小为 16 字节
#define AES_BLOCK_SIZE (16)

// 定义 AVAESCTR 结构体
typedef struct AVAESCTR {
    DECLARE_ALIGNED(8, uint8_t, counter)[AES_BLOCK_SIZE];
    DECLARE_ALIGNED(8, uint8_t, encrypted_counter)[AES_BLOCK_SIZE];
    AVAES aes;
} AVAESCTR;

// 分配并初始化 AVAESCTR 结构体的内存空间
struct AVAESCTR *av_aes_ctr_alloc(void)
{
    // 使用 av_mallocz 分配内存并初始化为 0
    return av_mallocz(sizeof(struct AVAESCTR));
}

// 设置初始向量（IV）
void av_aes_ctr_set_iv(struct AVAESCTR *a, const uint8_t* iv)
{
    // 复制 IV 到计数器的前部分
    memcpy(a->counter, iv, AES_CTR_IV_SIZE);
    // 其余部分清零
    memset(a->counter + AES_CTR_IV_SIZE, 0, sizeof(a->counter) - AES_CTR_IV_SIZE);
}

// 设置完整的初始向量
void av_aes_ctr_set_full_iv(struct AVAESCTR *a, const uint8_t* iv)
{
    // 完整复制 IV 到计数器
    memcpy(a->counter, iv, sizeof(a->counter));
}

// 获取初始向量
const uint8_t* av_aes_ctr_get_iv(struct AVAESCTR *a)
{
    // 返回计数器，即初始向量
    return a->counter;
}

// 设置随机的初始向量
void av_aes_ctr_set_random_iv(struct AVAESCTR *a)
{
    // 定义两个 32 位无符号整数的数组
    uint32_t iv[2];

    // 获取随机数种子并赋值给 iv 数组
    iv[0] = av_get_random_seed();
    iv[1] = av_get_random_seed();

    // 设置初始向量
    av_aes_ctr_set_iv(a, (uint8_t*)iv);
}

// 初始化 AVAESCTR 结构体
int av_aes_ctr_init(struct AVAESCTR *a, const uint8_t *key)
{
    // 初始化 AES 结构体
    av_aes_init(&a->aes, key, 128, 0);

    // 计数器清零
    memset(a->counter, 0, sizeof(a->counter));

    // 返回 0 表示成功
    return 0;
}

// 释放 AVAESCTR 结构体的内存
void av_aes_ctr_free(struct AVAESCTR *a)
{
    // 释放内存
    av_free(a);
}

static inline void av_aes_ctr_increment_be64(uint8_t* counter)
{
    uint64_t c = AV_RB64A(counter) + 1;
    AV_WB64A(counter, c);
}

// 递增初始向量
void av_aes_ctr_increment_iv(struct AVAESCTR *a)
{
    // 递增计数器
    av_aes_ctr_increment_be64(a->counter);
    // 计数器的后半部分清零
    memset(a->counter + AES_CTR_IV_SIZE, 0, sizeof(a->counter) - AES_CTR_IV_SIZE);
}

// 加密或解密数据
void av_aes_ctr_crypt(struct AVAESCTR *a, uint8_t *dst, const uint8_t *src, int count)
{
    while (count >= AES_BLOCK_SIZE) {
        av_aes_crypt(&a->aes, a->encrypted_counter, a->counter, 1, NULL, 0);
        av_aes_ctr_increment_be64(a->counter + 8);
#if HAVE_FAST_64BIT
        for (int len = 0; len < AES_BLOCK_SIZE; len += 8)
            AV_WN64(&dst[len], AV_RN64(&src[len]) ^ AV_RN64A(&a->encrypted_counter[len]));
#else
        for (int len = 0; len < AES_BLOCK_SIZE; len += 4)
            AV_WN32(&dst[len], AV_RN32(&src[len]) ^ AV_RN32A(&a->encrypted_counter[len]));
#endif
        dst += AES_BLOCK_SIZE;
        src += AES_BLOCK_SIZE;
        count -= AES_BLOCK_SIZE;
    }

    if (count > 0) {
        av_aes_crypt(&a->aes, a->encrypted_counter, a->counter, 1, NULL, 0);
        av_aes_ctr_increment_be64(a->counter + 8);
        for (int len = 0; len < count; len++)
            dst[len] = src[len] ^ a->encrypted_counter[len];
    }
}
