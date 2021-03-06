/*
 * Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <stdio.h>
#include <string.h>
#include "jvmti.h"
#include "agent_common.h"
#include "JVMTITools.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JNI_ENV_ARG

#ifdef __cplusplus
#define JNI_ENV_ARG(x, y) y
#define JNI_ENV_PTR(x) x
#else
#define JNI_ENV_ARG(x,y) x, y
#define JNI_ENV_PTR(x) (*x)
#endif

#endif

#define PASSED 0
#define STATUS_FAILED 2
#define FAILED_NO_OOM 3
#define MAX_CHUNK 1024*1024

// Limit total allocations to 8Gb.
// Without this check we will loop forever if the OS does not
// limit virtual memory (this usually happens on mac).
#define MAX_CHUNK_COUNT 8*1024

static jvmtiEnv *jvmti = NULL;
static jint result = PASSED;
static jboolean printdump = JNI_FALSE;

#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_alloc001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_alloc001(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_alloc001(JavaVM *jvm, char *options, void *reserved) {
    return JNI_VERSION_1_8;
}
#endif
jint Agent_Initialize(JavaVM *jvm, char *options, void *reserved) {
    jint res;

    if (options != NULL && strcmp(options, "printdump") == 0) {
        printdump = JNI_TRUE;
    }

    res = JNI_ENV_PTR(jvm)->
        GetEnv(JNI_ENV_ARG(jvm, (void **) &jvmti), JVMTI_VERSION_1_1);
    if (res != JNI_OK || jvmti == NULL) {
        printf("Wrong result of a valid call to GetEnv!\n");
        return JNI_ERR;
    }

    return JNI_OK;
}

JNIEXPORT jint JNICALL
Java_nsk_jvmti_Allocate_alloc001_check(JNIEnv *env, jclass cls) {
    jvmtiError err;
    size_t size;
    void *prev = NULL;
    void **mem;
    int memCount = 1;

    if (jvmti == NULL) {
        printf("JVMTI client was not properly loaded!\n");
        return STATUS_FAILED;
    }

    if (printdump == JNI_TRUE) {
        printf(">>> Null pointer check ...\n");
    }
    err = (*jvmti)->Allocate(jvmti, (jlong)1, NULL);
    if (err != JVMTI_ERROR_NULL_POINTER) {
        printf("Error expected: JVMTI_ERROR_NULL_POINTER, got: %s\n",
               TranslateError(err));
        result = STATUS_FAILED;
    }
    if (printdump == JNI_TRUE) {
        printf(">>> ... done\n");
    }

    if (printdump == JNI_TRUE) {
        printf(">>> Accessibility check ...\n");
    }
    for (size = sizeof(mem); size <= MAX_CHUNK; size <<= 1) {
        err = (*jvmti)->Allocate(jvmti, size, (unsigned char **)&mem);
        if (err == JVMTI_ERROR_NONE) {
            memset(mem, 0, size);
            *mem = prev;
            prev = mem;
        } else if (err == JVMTI_ERROR_OUT_OF_MEMORY) {
            break;
        } else {
            printf("(Allocate) Error expected: JVMTI_ERROR_NONE, got: %s\n",
                   TranslateError(err));
            result = STATUS_FAILED;
            break;
        }
    }
    if (printdump == JNI_TRUE) {
        printf(">>> ... done\n");
    }

    if (printdump == JNI_TRUE) {
        printf(">>> Out of memory check ...\n");
    }
    while (err != JVMTI_ERROR_OUT_OF_MEMORY) {
        err = (*jvmti)->Allocate(jvmti, (jlong)MAX_CHUNK, (unsigned char **)&mem);
        if (err == JVMTI_ERROR_NONE) {
            *mem = prev;
            prev = mem;
            memCount++;
            if (memCount > MAX_CHUNK_COUNT) {
                printf("Allocated %dMb. Virtual memory limit too high. Quit to avoid timeout.\n", memCount);
                result = FAILED_NO_OOM;
                break;
            }
        } else if (err == JVMTI_ERROR_OUT_OF_MEMORY) {
            break;
        } else {
            printf("Error expected: JVMTI_ERROR_OUT_OF_MEMORY, got: %s\n",
                   TranslateError(err));
            result = STATUS_FAILED;
            break;
        }

        if (printdump == JNI_TRUE && (memCount % 50 == 0) ) {
           printf(">>> ... done (%dMb)\n", memCount);
        }
    }
    if (printdump == JNI_TRUE) {
        printf(">>> ... done (%dMb)\n", memCount);
    }

    if (printdump == JNI_TRUE) {
        printf(">>> Deallocation ...\n");
    }
    while (prev != NULL) {
        mem = prev;
        prev = *mem;
        err = (*jvmti)->Deallocate(jvmti, (unsigned char *)mem);
        if (err != JVMTI_ERROR_NONE) {
            printf("(Deallocate) Error expected: JVMTI_ERROR_NONE, got: %s\n",
                   TranslateError(err));
            result = STATUS_FAILED;
            break;
        }
    }
    if (printdump == JNI_TRUE) {
        printf(">>> ... done\n");
    }

    return result;
}

#ifdef __cplusplus
}
#endif
