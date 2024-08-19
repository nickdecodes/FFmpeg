/*
 * copyright (c) 2015 rcombs
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
 * 版权声明，作者为 2015 年的 rcombs
 */

#ifndef AVUTIL_AES_INTERNAL_H
// 如果没有定义 AVUTIL_AES_INTERNAL_H 这个宏
#define AVUTIL_AES_INTERNAL_H
// 开始定义

#include "mem_internal.h"
// 包含内部内存相关的头文件
#include <stdint.h>
// 包含标准整数类型头文件

// 定义一个联合类型 av_aes_block
typedef union {
    // 可以看作两个 64 位无符号整数
    uint64_t u64[2];
    // 可以看作四个 32 位无符号整数
    uint32_t u32[4];
    // 可以看作 4 个 4 字节的数组
    uint8_t u8x4[4][4];
    // 可以看作 16 个字节的数组
    uint8_t u8[16];
} av_aes_block;

// 定义 AVAES 结构体
typedef struct AVAES {
    // 声明一个 16 字节对齐的 av_aes_block 类型的数组 round_key，长度为 15
    DECLARE_ALIGNED(16, av_aes_block, round_key)[15];
    // 声明一个 16 字节对齐的 av_aes_block 类型的数组 state，长度为 2
    DECLARE_ALIGNED(16, av_aes_block, state)[2];
    // 加密轮数
    int rounds;
    // 加密函数指针
    void (*crypt)(struct AVAES *a, uint8_t *dst, const uint8_t *src, int count, uint8_t *iv, int rounds);
} AVAES;

#endif /* AVUTIL_AES_INTERNAL_H */
// 结束如果没有定义 AVUTIL_AES_INTERNAL_H 的条件编译
