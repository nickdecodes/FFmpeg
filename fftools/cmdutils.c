/*
 * Various utilities for command line tools
 * Copyright (c) 2000-2003 Fabrice Bellard
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

/* Include only the enabled headers since some compilers (namely, Sun
   Studio) will not omit unused inline functions and create undefined
   references to libraries that are not being built. */

#include "config.h"
#include "compat/va_copy.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/bprint.h"
#include "libavutil/display.h"
#include "libavutil/getenv_utf8.h"
#include "libavutil/libm.h"
#include "libavutil/mem.h"
#include "libavutil/parseutils.h"
#include "libavutil/eval.h"
#include "libavutil/dict.h"
#include "libavutil/opt.h"
#include "cmdutils.h"
#include "fopen_utf8.h"
#include "opt_common.h"
#ifdef _WIN32
#include <windows.h>
#include "compat/w32dlfcn.h"
#endif

AVDictionary *sws_dict;
AVDictionary *swr_opts;
AVDictionary *format_opts, *codec_opts;

int hide_banner = 0;

void uninit_opts(void)
{
    av_dict_free(&swr_opts);
    av_dict_free(&sws_dict);
    av_dict_free(&format_opts);
    av_dict_free(&codec_opts);
}

void log_callback_help(void *ptr, int level, const char *fmt, va_list vl)
{
    vfprintf(stdout, fmt, vl);
}

void init_dynload(void)  // 定义一个名为 init_dynload 的无返回值函数，且函数没有参数
{
#if HAVE_SETDLLDIRECTORY && defined(_WIN32)  // 如果定义了 HAVE_SETDLLDIRECTORY 并且是 Windows 平台
    /* Calling SetDllDirectory with the empty string (but not NULL) removes the
     * current working directory from the DLL search path as a security pre-caution. */
    SetDllDirectory("");  // 调用 SetDllDirectory 函数并传入空字符串，以从 DLL 搜索路径中移除当前工作目录，作为一种安全预防措施
#endif
}

int parse_number(const char *context, const char *numstr, enum OptionType type,
                 double min, double max, double *dst)
{
    // 用于解析数字字符串后的剩余部分指针
    char *tail;
    // 错误信息字符串
    const char *error;

    // 将数字字符串转换为双精度浮点数，并获取剩余未转换部分的指针
    double d = av_strtod(numstr, &tail);

    // 如果转换后数字字符串还有剩余部分（不是纯数字）
    if (*tail)
        error = "Expected number for %s but found: %s\n";
    // 如果转换后的数值不在指定的最小和最大范围之间
    else if (d < min || d > max)
        error = "The value for %s was %s which is not within %f - %f\n";
    // 如果要求是 64 位整数类型，但转换后的数值不能准确表示为 64 位整数
    else if (type == OPT_TYPE_INT64 && (int64_t)d != d)
        error = "Expected int64 for %s but found %s\n";
    // 如果要求是普通整数类型，但转换后的数值不能准确表示为普通整数
    else if (type == OPT_TYPE_INT && (int)d != d)
        error = "Expected int for %s but found %s\n";
    // 如果转换后的数值没有上述问题
    else {
        // 将转换后的数值存储到指定的目标地址
        *dst = d;
        return 0;  // 表示转换成功，返回 0
    }

    // 记录致命错误日志，输出错误信息
    av_log(NULL, AV_LOG_FATAL, error, context, numstr, min, max);
    return AVERROR(EINVAL);  // 返回错误码，表示输入的数字格式或范围错误
}

void show_help_options(const OptionDef *options, const char *msg, int req_flags,
                       int rej_flags)
{
    const OptionDef *po;
    int first;

    first = 1;
    for (po = options; po->name; po++) {
        char buf[128];

        if (((po->flags & req_flags) != req_flags) ||
            (po->flags & rej_flags))
            continue;

        if (first) {
            printf("%s\n", msg);
            first = 0;
        }
        av_strlcpy(buf, po->name, sizeof(buf));

        if (po->flags & OPT_FLAG_PERSTREAM)
            av_strlcat(buf, "[:<stream_spec>]", sizeof(buf));
        else if (po->flags & OPT_FLAG_SPEC)
            av_strlcat(buf, "[:<spec>]", sizeof(buf));

        if (po->argname)
            av_strlcatf(buf, sizeof(buf), " <%s>", po->argname);

        printf("-%-17s  %s\n", buf, po->help);
    }
    printf("\n");
}

void show_help_children(const AVClass *class, int flags)
{
    void *iter = NULL;
    const AVClass *child;
    if (class->option) {
        av_opt_show2(&class, NULL, flags, 0);
        printf("\n");
    }

    while (child = av_opt_child_class_iterate(class, &iter))
        show_help_children(child, flags);
}

static const OptionDef *find_option(const OptionDef *po, const char *name)  // 定义一个静态函数 find_option，返回指向 OptionDef 的常量指针，接受 OptionDef 指针 po 和字符串 name 作为参数
{
    if (*name == '/')  // 如果 name 的第一个字符是 '/'
        name++;  // 将 name 指针向前移动一位

    while (po->name) {  // 当 po 指向的 OptionDef 结构中的 name 字段不为空时（即还有未检查完的选项定义）
        const char *end;  // 定义一个字符指针 end
        if (av_strstart(name, po->name, &end) && (!*end || *end == ':'))  // 如果 name 以 po->name 开头，并且 end 指向的位置要么是空字符要么是 ':'
            break;  // 找到匹配的选项，退出循环
        po++;  // 未找到匹配，移动到下一个选项定义
    }
    return po;  // 返回找到的选项定义的指针，如果未找到则是循环结束时的指针
}

/* _WIN32 means using the windows libc - cygwin doesn't define that
 * by default. HAVE_COMMANDLINETOARGVW is true on cygwin, while
 * it doesn't provide the actual command line via GetCommandLineW(). */
#if HAVE_COMMANDLINETOARGVW && defined(_WIN32)
#include <shellapi.h>
/* Will be leaked on exit */
static char** win32_argv_utf8 = NULL;
static int win32_argc = 0;

/**
 * Prepare command line arguments for executable.
 * For Windows - perform wide-char to UTF-8 conversion.
 * Input arguments should be main() function arguments.
 * @param argc_ptr Arguments number (including executable)
 * @param argv_ptr Arguments list.
 */
static void prepare_app_arguments(int *argc_ptr, char ***argv_ptr)
{
    char *argstr_flat;
    wchar_t **argv_w;
    int i, buffsize = 0, offset = 0;

    if (win32_argv_utf8) {
        *argc_ptr = win32_argc;
        *argv_ptr = win32_argv_utf8;
        return;
    }

    win32_argc = 0;
    argv_w = CommandLineToArgvW(GetCommandLineW(), &win32_argc);
    if (win32_argc <= 0 || !argv_w)
        return;

    /* determine the UTF-8 buffer size (including NULL-termination symbols) */
    for (i = 0; i < win32_argc; i++)
        buffsize += WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1,
                                        NULL, 0, NULL, NULL);

    win32_argv_utf8 = av_mallocz(sizeof(char *) * (win32_argc + 1) + buffsize);
    argstr_flat     = (char *)win32_argv_utf8 + sizeof(char *) * (win32_argc + 1);
    if (!win32_argv_utf8) {
        LocalFree(argv_w);
        return;
    }

    for (i = 0; i < win32_argc; i++) {
        win32_argv_utf8[i] = &argstr_flat[offset];
        offset += WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1,
                                      &argstr_flat[offset],
                                      buffsize - offset, NULL, NULL);
    }
    win32_argv_utf8[i] = NULL;
    LocalFree(argv_w);

    *argc_ptr = win32_argc;
    *argv_ptr = win32_argv_utf8;
}
#else
static inline void prepare_app_arguments(int *argc_ptr, char ***argv_ptr)
{
    /* nothing to do */
}
#endif /* HAVE_COMMANDLINETOARGVW */

static int opt_has_arg(const OptionDef *o)
{
    // 如果选项的类型是 `OPT_TYPE_BOOL`（布尔型）
    if (o->type == OPT_TYPE_BOOL)
        // 返回 0，表示布尔型选项没有参数
        return 0;
    // 如果选项的类型是 `OPT_TYPE_FUNC`（函数类型）
    if (o->type == OPT_TYPE_FUNC)
        // 根据选项的标志（`o->flags`）中是否设置了 `OPT_FUNC_ARG` 标志来判断是否有参数
        // 使用 `!!` 进行双重逻辑非操作将结果转换为 0（假）或 1（真）
        return !!(o->flags & OPT_FUNC_ARG);
    // 如果选项既不是布尔型也不是函数类型
    // 返回 1，表示该选项有参数
    return 1;
}

static int write_option(void *optctx, const OptionDef *po, const char *opt,
                        const char *arg, const OptionDef *defs)
{
    // 根据选项标志判断目标存储位置
    // 如果选项标志设置了 `OPT_FLAG_OFFSET`，通过偏移量计算目标地址
    // 否则，使用选项中指定的目标指针地址
    void *dst = po->flags & OPT_FLAG_OFFSET ?
                (uint8_t *)optctx + po->u.off : po->u.dst_ptr;
    char *arg_allocated = NULL;  // 用于存储分配的参数内存

    SpecifierOptList *sol = NULL;  // 一个结构体指针，用于后续操作
    double num;  // 用于存储数值
    int ret = 0;  // 函数的返回值

    // 如果选项字符串以'/'开头
    if (*opt == '/') {
        opt++;  // 跳过开头的'/'

        // 如果选项类型是布尔型且尝试从文件加载参数
        if (po->type == OPT_TYPE_BOOL) {
            av_log(NULL, AV_LOG_FATAL,
                   "Requested to load an argument from file for a bool option '%s'\n",
                   po->name);  // 记录致命错误日志
            return AVERROR(EINVAL);  // 返回错误码，表示参数类型错误
        }

        // 从指定文件读取参数内容并存储到 `arg_allocated`
        arg_allocated = file_read(arg);
        if (!arg_allocated) {
            av_log(NULL, AV_LOG_FATAL,
                   "Error reading the value for option '%s' from file: %s\n",
                   opt, arg);  // 记录致命错误日志
            return AVERROR(EINVAL);  // 返回错误码，表示文件读取错误
        }

        arg = arg_allocated;  // 将读取的参数内容赋值给 `arg`
    }

    // 如果选项标志设置了 `OPT_FLAG_SPEC`
    if (po->flags & OPT_FLAG_SPEC) {
        char *p = strchr(opt, ':');  // 在选项字符串中查找 ':'
        char *str;

        sol = dst;  // 将 `dst` 赋值给 `sol`

        ret = GROW_ARRAY(sol->opt, sol->nb_opt);  // 扩展数组
        if (ret < 0)  // 如果扩展数组操作失败
            goto finish;  // 跳转到 `finish` 标签处

        str = av_strdup(p ? p + 1 : "");  // 复制字符串
        if (!str) {
            ret = AVERROR(ENOMEM);  // 设置错误码为内存不足
            goto finish;  // 跳转到 `finish` 标签处
        }
        sol->opt[sol->nb_opt - 1].specifier = str;  // 设置结构体中的指定字段
        dst = &sol->opt[sol->nb_opt - 1].u;  // 更新目标地址
    }

    // 如果选项类型是字符串类型
    if (po->type == OPT_TYPE_STRING) {
        char *str;
        // 如果有从文件分配的参数
        if (arg_allocated) {
            str = arg_allocated;  // 使用分配的参数
            arg_allocated = NULL;  // 释放相关资源
        } else
            str = av_strdup(arg);  // 复制参数字符串

        av_freep(dst);  // 释放原目标地址的内存

        if (!str) {
            ret = AVERROR(ENOMEM);  // 设置错误码为内存不足
            goto finish;  // 跳转到 `finish` 标签处
        }

        *(char **)dst = str;  // 将字符串指针存储到目标地址
    }
    // 如果选项类型是布尔型或整型
    else if (po->type == OPT_TYPE_BOOL || po->type == OPT_TYPE_INT) {
        // 解析参数为数字
        ret = parse_number(opt, arg, OPT_TYPE_INT64, INT_MIN, INT_MAX, &num);
        if (ret < 0)  // 如果解析失败
            goto finish;  // 跳转到 `finish` 标签处

        *(int *)dst = num;  // 将解析后的数字存储到目标地址
    }
    // 如果选项类型是 64 位整型
    else if (po->type == OPT_TYPE_INT64) {
        // 解析参数为 64 位整型数字
        ret = parse_number(opt, arg, OPT_TYPE_INT64, INT64_MIN, (double)INT64_MAX, &num);
        if (ret < 0)  // 如果解析失败
            goto finish;  // 跳转到 `finish` 标签处

        *(int64_t *)dst = num;  // 将解析后的 64 位整型数字存储到目标地址
    }
    // 如果选项类型是时间类型
    else if (po->type == OPT_TYPE_TIME) {
        ret = av_parse_time(dst, arg, 1);  // 解析时间参数
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Invalid duration for option %s: %s\n",
                   opt, arg);  // 记录错误日志
            goto finish;  // 跳转到 `finish` 标签处
        }
    }
    // 如果选项类型是单精度浮点型
    else if (po->type == OPT_TYPE_FLOAT) {
        // 解析参数为单精度浮点数
        ret = parse_number(opt, arg, OPT_TYPE_FLOAT, -INFINITY, INFINITY, &num);
        if (ret < 0)  // 如果解析失败
            goto finish;  // 跳转到 `finish` 标签处

        *(float *)dst = num;  // 将解析后的单精度浮点数存储到目标地址
    }
    // 如果选项类型是双精度浮点型
    else if (po->type == OPT_TYPE_DOUBLE) {
        // 解析参数为双精度浮点数
        ret = parse_number(opt, arg, OPT_TYPE_DOUBLE, -INFINITY, INFINITY, &num);
        if (ret < 0)  // 如果解析失败
            goto finish;  // 跳转到 `finish` 标签处

        *(double *)dst = num;  // 将解析后的双精度浮点数存储到目标地址
    }
    // 如果选项类型是函数类型且有函数参数
    else {
        av_assert0(po->type == OPT_TYPE_FUNC && po->u.func_arg);

        ret = po->u.func_arg(optctx, opt, arg);  // 调用函数参数处理函数
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Failed to set value '%s' for option '%s': %s\n",
                   arg, opt, av_err2str(ret));  // 记录错误日志
            goto finish;  // 跳转到 `finish` 标签处
        }
    }

    // 如果选项标志设置了 `OPT_EXIT`
    if (po->flags & OPT_EXIT) {
        ret = AVERROR_EXIT;  // 设置错误码为退出相关错误
        goto finish;  // 跳转到 `finish` 标签处
    }

    // 如果 `sol` 不为空
    if (sol) {
        sol->type = po->type;  // 设置结构体的类型字段
        sol->opt_canon = (po->flags & OPT_HAS_CANON)?
                         find_option(defs, po->u1.name_canon) : po;  // 设置相关选项
    }

finish:
    // 释放分配的参数内存
    av_freep(&arg_allocated);
    return ret;  // 返回函数的结果
}

int parse_option(void *optctx, const char *opt, const char *arg,
                 const OptionDef *options)
{
    // 定义一个静态的 OptionDef 结构体变量 opt_avoptions ，用于特定的选项配置
    static const OptionDef opt_avoptions = {
       .name       = "AVOption passthrough",  // 选项的名称
       .type       = OPT_TYPE_FUNC,  // 选项的类型为函数类型
       .flags      = OPT_FUNC_ARG,  // 选项的标志
       .u.func_arg = opt_default  // 与函数类型相关的函数参数
    };

    const OptionDef *po;  // 定义一个指向 OptionDef 结构体的指针 po
    int ret;  // 定义一个整型变量 ret 用于存储函数的返回值

    // 在给定的选项列表 options 中查找与输入的选项字符串 opt 匹配的选项
    po = find_option(options, opt);
    // 如果未找到匹配的选项，并且选项字符串以 "no" 开头
    if (!po->name && opt[0] == 'n' && opt[1] == 'o') {
        // 在选项列表中查找去除前两个字符（"no"）后的选项
        po = find_option(options, opt + 2);
        // 如果找到的选项的类型为布尔型
        if ((po->name && po->type == OPT_TYPE_BOOL))
            arg = "0";  // 将参数设置为 "0"
    } else if (po->type == OPT_TYPE_BOOL)  // 如果找到的选项是布尔型选项
        arg = "1";  // 将参数设置为 "1"

    // 如果未找到匹配的选项名称
    if (!po->name)
        po = &opt_avoptions;  // 将 po 指向之前定义的静态选项结构体 opt_avoptions
    // 如果仍然未找到匹配的选项名称
    if (!po->name) {
        // 记录错误日志，表示遇到无法识别的选项
        av_log(NULL, AV_LOG_ERROR, "Unrecognized option '%s'\n", opt);
        return AVERROR(EINVAL);  // 返回错误码，表示输入的选项无效
    }
    // 如果选项需要参数但没有提供参数
    if (opt_has_arg(po) && !arg) {
        // 记录错误日志，表示选项缺少参数
        av_log(NULL, AV_LOG_ERROR, "Missing argument for option '%s'\n", opt);
        return AVERROR(EINVAL);  // 返回错误码，表示参数缺失错误
    }

    // 调用 write_option 函数来处理选项
    ret = write_option(optctx, po, opt, arg, options);
    // 如果 write_option 函数返回负值（表示出现错误）
    if (ret < 0)
        return ret;  // 直接返回该错误

    // 返回一个表示选项是否具有参数的整数值
    return opt_has_arg(po);
}

int parse_options(void *optctx, int argc, char **argv, const OptionDef *options,
                  int (*parse_arg_function)(void *, const char*))  // 定义一个函数 parse_options，返回整数类型，接受多个参数
{
    const char *opt;  // 定义一个指向字符的常量指针 opt
    int optindex, handleoptions = 1, ret;  // 定义整数变量 optindex、handleoptions 并初始化为 1，以及 ret

    /* perform system-dependent conversions for arguments list */
    prepare_app_arguments(&argc, &argv);  // 执行与系统相关的参数列表转换

    /* parse options */
    optindex = 1;  // 初始化选项索引为 1
    while (optindex < argc) {  // 当选项索引小于参数数量时
        opt = argv[optindex++];  // 获取当前选项并将选项索引递增

        if (handleoptions && opt[0] == '-' && opt[1] != '\0') {  // 如果正在处理选项，并且选项以'-'开头且第二个字符不为空
            if (opt[1] == '-' && opt[2] == '\0') {  // 如果是 '--' 且后面没有字符
                handleoptions = 0;  // 停止处理选项
                continue;  // 继续下一次循环
            }
            opt++;  // 跳过开头的 '-'

            if ((ret = parse_option(optctx, opt, argv[optindex], options)) < 0)  // 解析选项
                return ret;  // 如果解析结果为负数（错误），则返回
            optindex += ret;  // 根据解析结果调整选项索引
        } else {  // 如果不是选项
            if (parse_arg_function) {  // 如果提供了解析参数的函数
                ret = parse_arg_function(optctx, opt);  // 调用该函数解析参数
                if (ret < 0)  // 如果返回值为负数（错误）
                    return ret;  // 则返回
            }
        }
    }

    return 0;  // 正常结束，返回 0
}

int parse_optgroup(void *optctx, OptionGroup *g, const OptionDef *defs)
{
    int i, ret;

    av_log(NULL, AV_LOG_DEBUG, "Parsing a group of options: %s %s.\n",
           g->group_def->name, g->arg);

    for (i = 0; i < g->nb_opts; i++) {
        Option *o = &g->opts[i];

        if (g->group_def->flags &&
            !(g->group_def->flags & o->opt->flags)) {
            av_log(NULL, AV_LOG_ERROR, "Option %s (%s) cannot be applied to "
                   "%s %s -- you are trying to apply an input option to an "
                   "output file or vice versa. Move this option before the "
                   "file it belongs to.\n", o->key, o->opt->help,
                   g->group_def->name, g->arg);
            return AVERROR(EINVAL);
        }

        av_log(NULL, AV_LOG_DEBUG, "Applying option %s (%s) with argument %s.\n",
               o->key, o->opt->help, o->val);

        ret = write_option(optctx, o->opt, o->key, o->val, defs);
        if (ret < 0)
            return ret;
    }

    av_log(NULL, AV_LOG_DEBUG, "Successfully parsed a group of options.\n");

    return 0;
}

int locate_option(int argc, char **argv, const OptionDef *options, 
                  const char *optname)  // 定义一个名为 locate_option 的函数，返回整数，接受参数 argc（命令行参数数量）、argv（命令行参数数组）、options（选项定义）和 optname（要查找的选项名称）
{
    const OptionDef *po;  // 定义一个指向 OptionDef 结构体的常量指针 po
    int i;  // 定义一个整数变量 i

    for (i = 1; i < argc; i++) {  // 从第二个命令行参数开始遍历
        const char *cur_opt = argv[i];  // 获取当前参数

        if (*cur_opt++ != '-')  // 如果当前参数的第一个字符不是'-'，跳过
            continue;

        po = find_option(options, cur_opt);  // 调用 find_option 函数查找与当前参数对应的选项定义，并将结果存储在 po 中
        if (!po->name && cur_opt[0] == 'n' && cur_opt[1] == 'o')  // 如果未找到匹配的选项定义，并且当前参数以 "no" 开头
            po = find_option(options, cur_opt + 2);  // 再次查找从第三个字符开始的部分

        if ((!po->name && !strcmp(cur_opt, optname)) ||  // 如果未找到匹配的选项定义但当前参数与要查找的选项名称相同，或者
             (po->name && !strcmp(optname, po->name)))  // 找到了匹配的选项定义且选项名称与要查找的相同
            return i;  // 返回当前参数的索引

        if (!po->name || opt_has_arg(po))  // 如果未找到匹配的选项定义或者该选项需要参数
            i++;  // 下一个参数也可能是当前选项的参数，所以索引加 1
    }
    return 0;  // 未找到指定选项，返回 0
}

static void dump_argument(FILE *report_file, const char *a)
{
    const unsigned char *p;

    for (p = a; *p; p++)
        if (!((*p >= '+' && *p <= ':') || (*p >= '@' && *p <= 'Z') ||
              *p == '_' || (*p >= 'a' && *p <= 'z')))
            break;
    if (!*p) {
        fputs(a, report_file);
        return;
    }
    fputc('"', report_file);
    for (p = a; *p; p++) {
        if (*p == '\\' || *p == '"' || *p == '$' || *p == '`')
            fprintf(report_file, "\\%c", *p);
        else if (*p < ' ' || *p > '~')
            fprintf(report_file, "\\x%02x", *p);
        else
            fputc(*p, report_file);
    }
    fputc('"', report_file);
}

static void check_options(const OptionDef *po)  // 定义一个静态的无返回值函数 check_options，接受一个指向 OptionDef 的常量指针 po
{
    while (po->name) {  // 当 po 指向的 OptionDef 结构中的 name 字段不为空时
        if (po->flags & OPT_PERFILE)  // 如果选项的标志包含 OPT_PERFILE
            av_assert0(po->flags & (OPT_INPUT | OPT_OUTPUT | OPT_DECODER));  // 断言该选项的标志也包含 OPT_INPUT 、 OPT_OUTPUT 或 OPT_DECODER 中的至少一个

        if (po->type == OPT_TYPE_FUNC)  // 如果选项类型是 OPT_TYPE_FUNC
            av_assert0(!(po->flags & (OPT_FLAG_OFFSET | OPT_FLAG_SPEC)));  // 断言该选项的标志不包含 OPT_FLAG_OFFSET 或 OPT_FLAG_SPEC

        // OPT_FUNC_ARG 只能为 OPT_TYPE_FUNC 类型的选项设置
        av_assert0((po->type == OPT_TYPE_FUNC) ||!(po->flags & OPT_FUNC_ARG));  // 断言要么选项类型是 OPT_TYPE_FUNC ，要么标志中不包含 OPT_FUNC_ARG

        po++;  // 移动到下一个 OptionDef 结构
    }
}

void parse_loglevel(int argc, char **argv, const OptionDef *options)  // 定义一个名为 parse_loglevel 的函数，接受参数 argc（命令行参数数量）、argv（命令行参数数组）和 options（选项定义）
{
    int idx = locate_option(argc, argv, options, "loglevel");  // 调用 locate_option 函数查找 "loglevel" 选项在命令行参数中的位置，并将结果存储在 idx 中
    char *env;  // 定义一个字符指针 env

    check_options(options);  // 调用 check_options 函数检查选项

    if (!idx)  // 如果未找到 "loglevel" 选项
        idx = locate_option(argc, argv, options, "v");  // 尝试查找 "v" 选项的位置
    if (idx && argv[idx + 1])  // 如果找到了选项并且其后有值
        opt_loglevel(NULL, "loglevel", argv[idx + 1]);  // 调用 opt_loglevel 函数处理日志级别
    idx = locate_option(argc, argv, options, "report");  // 查找 "report" 选项的位置
    env = getenv_utf8("FFREPORT");  // 获取环境变量 "FFREPORT" 的值
    if (env || idx) {  // 如果环境变量存在或者找到了 "report" 选项
        FILE *report_file = NULL;  // 定义一个文件指针 report_file 并初始化为 NULL
        init_report(env, &report_file);  // 调用 init_report 函数初始化报告，并传递环境变量和文件指针
        if (report_file) {  // 如果文件指针有效
            int i;  // 定义一个整数变量 i
            fprintf(report_file, "Command line:\n");  // 向报告文件写入 "Command line:"
            for (i = 0; i < argc; i++) {  // 遍历命令行参数
                dump_argument(report_file, argv[i]);  // 将每个参数写入报告文件
                fputc(i < argc - 1 ? ' ' : '\n', report_file);  // 根据是否是最后一个参数决定写入空格或换行符
            }
            fflush(report_file);  // 刷新报告文件的缓冲区
        }
    }
    freeenv_utf8(env);  // 释放环境变量获取到的内存
    idx = locate_option(argc, argv, options, "hide_banner");  // 查找 "hide_banner" 选项的位置
    if (idx)  // 如果找到了
        hide_banner = 1;  // 设置 hide_banner 为 1
}

static const AVOption *opt_find(void *obj, const char *name, const char *unit,
                            int opt_flags, int search_flags)
{
    const AVOption *o = av_opt_find(obj, name, unit, opt_flags, search_flags);
    if(o && !o->flags)
        return NULL;
    return o;
}

#define FLAGS ((o->type == AV_OPT_TYPE_FLAGS && (arg[0]=='-' || arg[0]=='+')) ? AV_DICT_APPEND : 0)
int opt_default(void *optctx, const char *opt, const char *arg)
{
    const AVOption *o;
    int consumed = 0;
    char opt_stripped[128];
    const char *p;
    const AVClass *cc = avcodec_get_class(), *fc = avformat_get_class();
#if CONFIG_SWSCALE
    const AVClass *sc = sws_get_class();
#endif
#if CONFIG_SWRESAMPLE
    const AVClass *swr_class = swr_get_class();
#endif

    if (!strcmp(opt, "debug") || !strcmp(opt, "fdebug"))
        av_log_set_level(AV_LOG_DEBUG);

    if (!(p = strchr(opt, ':')))
        p = opt + strlen(opt);
    av_strlcpy(opt_stripped, opt, FFMIN(sizeof(opt_stripped), p - opt + 1));

    if ((o = opt_find(&cc, opt_stripped, NULL, 0,
                         AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ)) ||
        ((opt[0] == 'v' || opt[0] == 'a' || opt[0] == 's') &&
         (o = opt_find(&cc, opt + 1, NULL, 0, AV_OPT_SEARCH_FAKE_OBJ)))) {
        av_dict_set(&codec_opts, opt, arg, FLAGS);
        consumed = 1;
    }
    if ((o = opt_find(&fc, opt, NULL, 0,
                         AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ))) {
        av_dict_set(&format_opts, opt, arg, FLAGS);
        if (consumed)
            av_log(NULL, AV_LOG_VERBOSE, "Routing option %s to both codec and muxer layer\n", opt);
        consumed = 1;
    }
#if CONFIG_SWSCALE
    if (!consumed && (o = opt_find(&sc, opt, NULL, 0,
                         AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ))) {
        if (!strcmp(opt, "srcw") || !strcmp(opt, "srch") ||
            !strcmp(opt, "dstw") || !strcmp(opt, "dsth") ||
            !strcmp(opt, "src_format") || !strcmp(opt, "dst_format")) {
            av_log(NULL, AV_LOG_ERROR, "Directly using swscale dimensions/format options is not supported, please use the -s or -pix_fmt options\n");
            return AVERROR(EINVAL);
        }
        av_dict_set(&sws_dict, opt, arg, FLAGS);

        consumed = 1;
    }
#else
    if (!consumed && !strcmp(opt, "sws_flags")) {
        av_log(NULL, AV_LOG_WARNING, "Ignoring %s %s, due to disabled swscale\n", opt, arg);
        consumed = 1;
    }
#endif
#if CONFIG_SWRESAMPLE
    if (!consumed && (o=opt_find(&swr_class, opt, NULL, 0,
                                    AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ))) {
        av_dict_set(&swr_opts, opt, arg, FLAGS);
        consumed = 1;
    }
#endif

    if (consumed)
        return 0;
    return AVERROR_OPTION_NOT_FOUND;
}

/*
 * Check whether given option is a group separator.
 *
 * @return index of the group definition that matched or -1 if none
 */
static int match_group_separator(const OptionGroupDef *groups, int nb_groups,
                                 const char *opt)
{
    int i;

    for (i = 0; i < nb_groups; i++) {
        const OptionGroupDef *p = &groups[i];
        if (p->sep && !strcmp(p->sep, opt))
            return i;
    }

    return -1;
}

/*
 * Finish parsing an option group.
 *
 * @param group_idx which group definition should this group belong to
 * @param arg argument of the group delimiting option
 */
static int finish_group(OptionParseContext *octx, int group_idx,
                        const char *arg)
{
    OptionGroupList *l = &octx->groups[group_idx];
    OptionGroup *g;
    int ret;

    ret = GROW_ARRAY(l->groups, l->nb_groups);
    if (ret < 0)
        return ret;

    g = &l->groups[l->nb_groups - 1];

    *g             = octx->cur_group;
    g->arg         = arg;
    g->group_def   = l->group_def;
    g->sws_dict    = sws_dict;
    g->swr_opts    = swr_opts;
    g->codec_opts  = codec_opts;
    g->format_opts = format_opts;

    codec_opts  = NULL;
    format_opts = NULL;
    sws_dict    = NULL;
    swr_opts    = NULL;

    memset(&octx->cur_group, 0, sizeof(octx->cur_group));

    return ret;
}

/*
 * Add an option instance to currently parsed group.
 */
static int add_opt(OptionParseContext *octx, const OptionDef *opt,
                   const char *key, const char *val)
{
    int global = !(opt->flags & OPT_PERFILE);
    OptionGroup *g = global ? &octx->global_opts : &octx->cur_group;
    int ret;

    ret = GROW_ARRAY(g->opts, g->nb_opts);
    if (ret < 0)
        return ret;

    g->opts[g->nb_opts - 1].opt = opt;
    g->opts[g->nb_opts - 1].key = key;
    g->opts[g->nb_opts - 1].val = val;

    return 0;
}

static int init_parse_context(OptionParseContext *octx,
                              const OptionGroupDef *groups, int nb_groups)
{
    static const OptionGroupDef global_group = { "global" };
    int i;

    memset(octx, 0, sizeof(*octx));

    octx->groups    = av_calloc(nb_groups, sizeof(*octx->groups));
    if (!octx->groups)
        return AVERROR(ENOMEM);
    octx->nb_groups = nb_groups;

    for (i = 0; i < octx->nb_groups; i++)
        octx->groups[i].group_def = &groups[i];

    octx->global_opts.group_def = &global_group;
    octx->global_opts.arg       = "";

    return 0;
}

void uninit_parse_context(OptionParseContext *octx)
{
    int i, j;

    for (i = 0; i < octx->nb_groups; i++) {
        OptionGroupList *l = &octx->groups[i];

        for (j = 0; j < l->nb_groups; j++) {
            av_freep(&l->groups[j].opts);
            av_dict_free(&l->groups[j].codec_opts);
            av_dict_free(&l->groups[j].format_opts);

            av_dict_free(&l->groups[j].sws_dict);
            av_dict_free(&l->groups[j].swr_opts);
        }
        av_freep(&l->groups);
    }
    av_freep(&octx->groups);

    av_freep(&octx->cur_group.opts);
    av_freep(&octx->global_opts.opts);

    uninit_opts();
}

int split_commandline(OptionParseContext *octx, int argc, char *argv[],
                      const OptionDef *options,
                      const OptionGroupDef *groups, int nb_groups)
{
    int ret;
    int optindex = 1;
    int dashdash = -2;

    /* perform system-dependent conversions for arguments list */
    prepare_app_arguments(&argc, &argv);

    ret = init_parse_context(octx, groups, nb_groups);
    if (ret < 0)
        return ret;

    av_log(NULL, AV_LOG_DEBUG, "Splitting the commandline.\n");

    while (optindex < argc) {
        const char *opt = argv[optindex++], *arg;
        const OptionDef *po;
        int ret, group_idx;

        av_log(NULL, AV_LOG_DEBUG, "Reading option '%s' ...", opt);

        if (opt[0] == '-' && opt[1] == '-' && !opt[2]) {
            dashdash = optindex;
            continue;
        }
        /* unnamed group separators, e.g. output filename */
        if (opt[0] != '-' || !opt[1] || dashdash+1 == optindex) {
            ret = finish_group(octx, 0, opt);
            if (ret < 0)
                return ret;

            av_log(NULL, AV_LOG_DEBUG, " matched as %s.\n", groups[0].name);
            continue;
        }
        opt++;

#define GET_ARG(arg)                                                           \
do {                                                                           \
    arg = argv[optindex++];                                                    \
    if (!arg) {                                                                \
        av_log(NULL, AV_LOG_ERROR, "Missing argument for option '%s'.\n", opt);\
        return AVERROR(EINVAL);                                                \
    }                                                                          \
} while (0)

        /* named group separators, e.g. -i */
        group_idx = match_group_separator(groups, nb_groups, opt);
        if (group_idx >= 0) {
            GET_ARG(arg);
            ret = finish_group(octx, group_idx, arg);
            if (ret < 0)
                return ret;

            av_log(NULL, AV_LOG_DEBUG, " matched as %s with argument '%s'.\n",
                   groups[group_idx].name, arg);
            continue;
        }

        /* normal options */
        po = find_option(options, opt);
        if (po->name) {
            if (po->flags & OPT_EXIT) {
                /* optional argument, e.g. -h */
                arg = argv[optindex++];
            } else if (opt_has_arg(po)) {
                GET_ARG(arg);
            } else {
                arg = "1";
            }

            ret = add_opt(octx, po, opt, arg);
            if (ret < 0)
                return ret;

            av_log(NULL, AV_LOG_DEBUG, " matched as option '%s' (%s) with "
                   "argument '%s'.\n", po->name, po->help, arg);
            continue;
        }

        /* AVOptions */
        if (argv[optindex]) {
            ret = opt_default(NULL, opt, argv[optindex]);
            if (ret >= 0) {
                av_log(NULL, AV_LOG_DEBUG, " matched as AVOption '%s' with "
                       "argument '%s'.\n", opt, argv[optindex]);
                optindex++;
                continue;
            } else if (ret != AVERROR_OPTION_NOT_FOUND) {
                av_log(NULL, AV_LOG_ERROR, "Error parsing option '%s' "
                       "with argument '%s'.\n", opt, argv[optindex]);
                return ret;
            }
        }

        /* boolean -nofoo options */
        if (opt[0] == 'n' && opt[1] == 'o' &&
            (po = find_option(options, opt + 2)) &&
            po->name && po->type == OPT_TYPE_BOOL) {
            ret = add_opt(octx, po, opt, "0");
            if (ret < 0)
                return ret;

            av_log(NULL, AV_LOG_DEBUG, " matched as option '%s' (%s) with "
                   "argument 0.\n", po->name, po->help);
            continue;
        }

        av_log(NULL, AV_LOG_ERROR, "Unrecognized option '%s'.\n", opt);
        return AVERROR_OPTION_NOT_FOUND;
    }

    if (octx->cur_group.nb_opts || codec_opts || format_opts)
        av_log(NULL, AV_LOG_WARNING, "Trailing option(s) found in the "
               "command: may be ignored.\n");

    av_log(NULL, AV_LOG_DEBUG, "Finished splitting the commandline.\n");

    return 0;
}

int read_yesno(void)
{
    int c = getchar();
    int yesno = (av_toupper(c) == 'Y');

    while (c != '\n' && c != EOF)
        c = getchar();

    return yesno;
}

FILE *get_preset_file(char *filename, size_t filename_size,
                      const char *preset_name, int is_path,
                      const char *codec_name)
{
    FILE *f = NULL;
    int i;
#if HAVE_GETMODULEHANDLE && defined(_WIN32)
    char *datadir = NULL;
#endif
    char *env_home = getenv_utf8("HOME");
    char *env_ffmpeg_datadir = getenv_utf8("FFMPEG_DATADIR");
    const char *base[3] = { env_ffmpeg_datadir,
                            env_home,   /* index=1(HOME) is special: search in a .ffmpeg subfolder */
                            FFMPEG_DATADIR, };

    if (is_path) {
        av_strlcpy(filename, preset_name, filename_size);
        f = fopen_utf8(filename, "r");
    } else {
#if HAVE_GETMODULEHANDLE && defined(_WIN32)
        wchar_t *datadir_w = get_module_filename(NULL);
        base[2] = NULL;

        if (wchartoutf8(datadir_w, &datadir))
            datadir = NULL;
        av_free(datadir_w);

        if (datadir)
        {
            char *ls;
            for (ls = datadir; *ls; ls++)
                if (*ls == '\\') *ls = '/';

            if (ls = strrchr(datadir, '/'))
            {
                ptrdiff_t datadir_len = ls - datadir;
                size_t desired_size = datadir_len + strlen("/ffpresets") + 1;
                char *new_datadir = av_realloc_array(
                    datadir, desired_size, sizeof *datadir);
                if (new_datadir) {
                    datadir = new_datadir;
                    datadir[datadir_len] = 0;
                    strncat(datadir, "/ffpresets",  desired_size - 1 - datadir_len);
                    base[2] = datadir;
                }
            }
        }
#endif
        for (i = 0; i < 3 && !f; i++) {
            if (!base[i])
                continue;
            snprintf(filename, filename_size, "%s%s/%s.ffpreset", base[i],
                     i != 1 ? "" : "/.ffmpeg", preset_name);
            f = fopen_utf8(filename, "r");
            if (!f && codec_name) {
                snprintf(filename, filename_size,
                         "%s%s/%s-%s.ffpreset",
                         base[i], i != 1 ? "" : "/.ffmpeg", codec_name,
                         preset_name);
                f = fopen_utf8(filename, "r");
            }
        }
    }

#if HAVE_GETMODULEHANDLE && defined(_WIN32)
    av_free(datadir);
#endif
    freeenv_utf8(env_ffmpeg_datadir);
    freeenv_utf8(env_home);
    return f;
}

int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
    int ret = avformat_match_stream_specifier(s, st, spec);
    if (ret < 0)
        av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
    return ret;
}

int filter_codec_opts(const AVDictionary *opts, enum AVCodecID codec_id,
                      AVFormatContext *s, AVStream *st, const AVCodec *codec,
                      AVDictionary **dst, AVDictionary **opts_used)
{
    AVDictionary    *ret = NULL;
    const AVDictionaryEntry *t = NULL;
    int            flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM
                                      : AV_OPT_FLAG_DECODING_PARAM;
    char          prefix = 0;
    const AVClass    *cc = avcodec_get_class();

    switch (st->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        prefix  = 'v';
        flags  |= AV_OPT_FLAG_VIDEO_PARAM;
        break;
    case AVMEDIA_TYPE_AUDIO:
        prefix  = 'a';
        flags  |= AV_OPT_FLAG_AUDIO_PARAM;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        prefix  = 's';
        flags  |= AV_OPT_FLAG_SUBTITLE_PARAM;
        break;
    }

    while (t = av_dict_iterate(opts, t)) {
        const AVClass *priv_class;
        char *p = strchr(t->key, ':');
        int used = 0;

        /* check stream specification in opt name */
        if (p) {
            int err = check_stream_specifier(s, st, p + 1);
            if (err < 0) {
                av_dict_free(&ret);
                return err;
            } else if (!err)
                continue;

            *p = 0;
        }

        if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
            !codec ||
            ((priv_class = codec->priv_class) &&
             av_opt_find(&priv_class, t->key, NULL, flags,
                         AV_OPT_SEARCH_FAKE_OBJ))) {
            av_dict_set(&ret, t->key, t->value, 0);
            used = 1;
        } else if (t->key[0] == prefix &&
                 av_opt_find(&cc, t->key + 1, NULL, flags,
                             AV_OPT_SEARCH_FAKE_OBJ)) {
            av_dict_set(&ret, t->key + 1, t->value, 0);
            used = 1;
        }

        if (p)
            *p = ':';

        if (used && opts_used)
            av_dict_set(opts_used, t->key, "", 0);
    }

    *dst = ret;
    return 0;
}

int setup_find_stream_info_opts(AVFormatContext *s,
                                AVDictionary *codec_opts,
                                AVDictionary ***dst)
{
    int ret;
    AVDictionary **opts;

    *dst = NULL;

    if (!s->nb_streams)
        return 0;

    opts = av_calloc(s->nb_streams, sizeof(*opts));
    if (!opts)
        return AVERROR(ENOMEM);

    for (int i = 0; i < s->nb_streams; i++) {
        ret = filter_codec_opts(codec_opts, s->streams[i]->codecpar->codec_id,
                                s, s->streams[i], NULL, &opts[i], NULL);
        if (ret < 0)
            goto fail;
    }
    *dst = opts;
    return 0;
fail:
    for (int i = 0; i < s->nb_streams; i++)
        av_dict_free(&opts[i]);
    av_freep(&opts);
    return ret;
}

int grow_array(void **array, int elem_size, int *size, int new_size)
{
    if (new_size >= INT_MAX / elem_size) {
        av_log(NULL, AV_LOG_ERROR, "Array too big.\n");
        return AVERROR(ERANGE);
    }
    if (*size < new_size) {
        uint8_t *tmp = av_realloc_array(*array, new_size, elem_size);
        if (!tmp)
            return AVERROR(ENOMEM);
        memset(tmp + *size*elem_size, 0, (new_size-*size) * elem_size);
        *size = new_size;
        *array = tmp;
        return 0;
    }
    return 0;
}

void *allocate_array_elem(void *ptr, size_t elem_size, int *nb_elems)
{
    void *new_elem;

    if (!(new_elem = av_mallocz(elem_size)) ||
        av_dynarray_add_nofree(ptr, nb_elems, new_elem) < 0)
        return NULL;
    return new_elem;
}

double get_rotation(const int32_t *displaymatrix)
{
    double theta = 0;
    if (displaymatrix)
        theta = -round(av_display_rotation_get(displaymatrix));

    theta -= 360*floor(theta/360 + 0.9/360);

    if (fabs(theta - 90*round(theta/90)) > 2)
        av_log(NULL, AV_LOG_WARNING, "Odd rotation angle.\n"
               "If you want to help, upload a sample "
               "of this file to https://streams.videolan.org/upload/ "
               "and contact the ffmpeg-devel mailing list. (ffmpeg-devel@ffmpeg.org)");

    return theta;
}

/* read file contents into a string */
char *file_read(const char *filename)
{
    AVIOContext *pb      = NULL;
    int ret = avio_open(&pb, filename, AVIO_FLAG_READ);
    AVBPrint bprint;
    char *str;

    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error opening file %s.\n", filename);
        return NULL;
    }

    av_bprint_init(&bprint, 0, AV_BPRINT_SIZE_UNLIMITED);
    ret = avio_read_to_bprint(pb, &bprint, SIZE_MAX);
    avio_closep(&pb);
    if (ret < 0) {
        av_bprint_finalize(&bprint, NULL);
        return NULL;
    }
    ret = av_bprint_finalize(&bprint, &str);
    if (ret < 0)
        return NULL;
    return str;
}

void remove_avoptions(AVDictionary **a, AVDictionary *b)
{
    const AVDictionaryEntry *t = NULL;

    while ((t = av_dict_iterate(b, t))) {
        av_dict_set(a, t->key, NULL, AV_DICT_MATCH_CASE);
    }
}

int check_avoptions(AVDictionary *m)
{
    const AVDictionaryEntry *t = av_dict_iterate(m, NULL);
    if (t) {
        av_log(NULL, AV_LOG_FATAL, "Option %s not found.\n", t->key);
        return AVERROR_OPTION_NOT_FOUND;
    }

    return 0;
}
