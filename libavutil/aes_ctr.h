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

#ifndef AVUTIL_AES_CTR_H
// 如果没有定义 AVUTIL_AES_CTR_H 这个宏
#define AVUTIL_AES_CTR_H
// 开始定义

/**
 * @defgroup lavu_aes_ctr AES-CTR
 * @ingroup lavu_crypto
 * @{
 */
// 定义一个名为 lavu_aes_ctr 的组，属于 lavu_crypto 组，并开始组的定义

#include <stdint.h>
// 包含标准整数类型头文件

#include "attributes.h"
// 包含属性相关的头文件

// 定义 AES 密钥大小为 16 字节
#define AES_CTR_KEY_SIZE (16)
// 定义 AES 初始向量（IV）大小为 8 字节
#define AES_CTR_IV_SIZE (8)

// 定义 AVAESCTR 结构体，但没有具体内容
struct AVAESCTR;

/**
 * Allocate an AVAESCTR context.
 */
// 说明下面的函数用于分配一个 AVAESCTR 上下文
struct AVAESCTR *av_aes_ctr_alloc(void);

/**
 * Initialize an AVAESCTR context.
 *
 * @param a The AVAESCTR context to initialize
 * @param key encryption key, must have a length of AES_CTR_KEY_SIZE
 */
// 说明下面的函数用于初始化 AVAESCTR 上下文，参数 a 是要初始化的上下文，key 是加密密钥，长度必须为 AES_CTR_KEY_SIZE
int av_aes_ctr_init(struct AVAESCTR *a, const uint8_t *key);

/**
 * Release an AVAESCTR context.
 *
 * @param a The AVAESCTR context
 */
// 说明下面的函数用于释放 AVAESCTR 上下文，参数 a 是要释放的上下文
void av_aes_ctr_free(struct AVAESCTR *a);

/**
 * Process a buffer using a previously initialized context.
 *
 * @param a The AVAESCTR context
 * @param dst destination array, can be equal to src
 * @param src source array, can be equal to dst
 * @param size the size of src and dst
 */
// 说明下面的函数用于使用先前初始化的上下文处理缓冲区，参数 a 是上下文，dst 是目标数组，src 是源数组，size 是数组的大小
void av_aes_ctr_crypt(struct AVAESCTR *a, uint8_t *dst, const uint8_t *src, int size);

/**
 * Get the current iv
 */
// 说明下面的函数用于获取当前的初始向量（IV）
const uint8_t* av_aes_ctr_get_iv(struct AVAESCTR *a);

/**
 * Generate a random iv
 */
// 说明下面的函数用于生成一个随机的初始向量（IV）
void av_aes_ctr_set_random_iv(struct AVAESCTR *a);

/**
 * Forcefully change the 8-byte iv
 */
// 说明下面的函数用于强制更改 8 字节的初始向量（IV）
void av_aes_ctr_set_iv(struct AVAESCTR *a, const uint8_t* iv);

/**
 * Forcefully change the "full" 16-byte iv, including the counter
 */
// 说明下面的函数用于强制更改完整的 16 字节初始向量（IV），包括计数器
void av_aes_ctr_set_full_iv(struct AVAESCTR *a, const uint8_t* iv);

/**
 * Increment the top 64 bit of the iv (performed after each frame)
 */
// 说明下面的函数用于递增初始向量（IV）的高 64 位（在每一帧之后执行）
void av_aes_ctr_increment_iv(struct AVAESCTR *a);

/**
 * @}
 */
// 结束组的定义

#endif /* AVUTIL_AES_CTR_H */
// 结束如果没有定义 AVUTIL_AES_CTR_H 的条件编译
