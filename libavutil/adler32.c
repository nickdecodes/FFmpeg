/*
 * Compute the Adler-32 checksum of a data stream.
 * This is a modified version based on adler32.c from the zlib library.
 *
 * Copyright (C) 1995 Mark Adler
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/**
 * @file
 * Computes the Adler-32 checksum of a data stream
 *
 * This is a modified version based on adler32.c from the zlib library.
 * @author Mark Adler
 * @ingroup lavu_adler32
 */

/*
 * 这是一段注释，介绍了该函数的用途是计算数据流的 Adler-32 校验和，是基于 zlib 库中 adler32.c 的修改版本，并包含了版权和使用限制等信息。
 */

/**
 * @file
 * 这部分是关于文件的描述，指出该文件计算数据流的 Adler-32 校验和，作者是 Mark Adler，并属于特定的组 lavu_adler32
 */

#include "config.h"
#include "adler32.h"
#include "intreadwrite.h"
#include "macros.h"

// 定义常量 BASE 为 65521，是一个较大的小于 65536 的素数
#define BASE 65521L 

// 定义宏 DO1，用于处理单个字节
#define DO1(buf)  { s1 += *buf++; s2 += s1; }

// 定义宏 DO4，通过调用 DO1 处理 4 个字节
#define DO4(buf)  DO1(buf); DO1(buf); DO1(buf); DO1(buf);

// 定义宏 DO16，通过调用 DO4 处理 16 个字节
#define DO16(buf) DO4(buf); DO4(buf); DO4(buf); DO4(buf);

// 定义函数 av_adler32_update，用于更新 Adler-32 校验和
AVAdler av_adler32_update(AVAdler adler, const uint8_t *buf, size_t len)
{
    // 将传入的 Adler-32 值的低 16 位赋给 s1
    unsigned long s1 = adler & 0xffff;
    // 将传入的 Adler-32 值的高 16 位赋给 s2
    unsigned long s2 = adler >> 16;

    // 当要处理的数据长度大于 0 时
    while (len > 0) {
#if HAVE_FAST_64BIT && HAVE_FAST_UNALIGNED &&!CONFIG_SMALL
        // 计算一个合适的长度 len2
        unsigned len2 = FFMIN((len - 1) & ~7, 23 * 8);
        if (len2) {
            // 初始化一些变量
            uint64_t a1 = 0;
            uint64_t a2 = 0;
            uint64_t b1 = 0;
            uint64_t b2 = 0;
            len -= len2;  // 更新剩余长度
            s2 += s1 * len2;  // 更新 s2

            // 处理一段长度为 len2 的数据
            while (len2 >= 8) {
                uint64_t v = AV_RN64(buf);  // 读取 64 位数据
                a2 += a1;
                b2 += b1;
                a1 += v & 0x00FF00FF00FF00FF;  // 进行一些计算
                b1 += (v >> 8) & 0x00FF00FF00FF00FF;  // 进行一些计算
                len2 -= 8;  // 更新剩余长度
                buf += 8;  // 指针移动
            }

            // 进行一系列复杂的计算和更新 s1、s2
            s1 += ((a1 + b1) * 0x1000100010001) >> 48;
            s2 += ((((a2 & 0xFFFF0000FFFF) + (b2 & 0xFFFF0000FFFF) + ((a2 >> 16) & 0xFFFF0000FFFF) + ((b2 >> 16) & 0xFFFF0000FFFF)) * 0x800000008) >> 32)
#if HAVE_BIGENDIAN
                 + 2 * ((b1 * 0x1000200030004) >> 48)
                 + ((a1 * 0x1000100010001) >> 48)
                 + 2 * ((a1 * 0x0000100020003) >> 48);
#else
                 + 2 * ((a1 * 0x4000300020001) >> 48)
                 + ((b1 * 0x1000100010001) >> 48)
                 + 2 * ((b1 * 0x3000200010000) >> 48);
#endif
        }
#else
        // 在不满足前面的条件时，处理长度大于 4 且 s2 小于特定值的数据
        while (len > 4 && s2 < (1U << 31)) {
            DO4(buf);  // 处理 4 个字节
            len -= 4;  // 更新剩余长度
        }
#endif
        DO1(buf);  // 处理 1 个字节
        len--;  // 更新剩余长度
        s1 %= BASE;  // 对 s1 取模
        s2 %= BASE;  // 对 s2 取模
    }
    // 返回更新后的 Adler-32 值
    return (s2 << 16) | s1;
}
