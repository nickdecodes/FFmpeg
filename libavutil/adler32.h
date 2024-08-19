/*
 * copyright (c) 2006 Mans Rullgard
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

/**
 * @file
 * @ingroup lavu_adler32
 * Public header for Adler-32 hash function implementation.
 */

/*
 * 这是一段版权声明，指出代码由 Mans Rullgard 在 2006 年创作
 */

/**
 * @file
 * 这部分说明该文件在 lavu_adler32 组内
 * Public header for Adler-32 hash function implementation.
 */

#ifndef AVUTIL_ADLER32_H
// 如果没有定义 AVUTIL_ADLER32_H 这个宏
#define AVUTIL_ADLER32_H
// 包含一些必要的头文件
#include <stddef.h>
#include <stdint.h>
#include "attributes.h"

/**
 * @defgroup lavu_adler32 Adler-32
 * @ingroup lavu_hash
 * 定义了一个名为 lavu_adler32 的组，属于 lavu_hash 组
 * Adler-32 hash function implementation.
 *
 * @{
 */

// 定义一个数据类型 AVAdler 为 uint32_t
typedef uint32_t AVAdler;

/**
 * Calculate the Adler32 checksum of a buffer.
 * 描述了一个函数的功能
 * Passing the return value to a subsequent av_adler32_update() call
 * allows the checksum of multiple buffers to be calculated as though
 * they were concatenated.
 * 说明将返回值传递给后续的 av_adler32_update() 调用，可以计算多个缓冲区的校验和，就好像它们是连接在一起的
 *
 * @param adler initial checksum value  参数 adler 是初始校验和值
 * @param buf   pointer to input buffer  参数 buf 是输入缓冲区的指针
 * @param len   size of input buffer  参数 len 是输入缓冲区的大小
 * @return      updated checksum  返回更新后的校验和
 */
AVAdler av_adler32_update(AVAdler adler, const uint8_t *buf,
                          size_t len) av_pure;

/**
 * @}
 */

#endif /* AVUTIL_ADLER32_H */
// 结束如果没有定义 AVUTIL_ADLER32_H 的条件编译