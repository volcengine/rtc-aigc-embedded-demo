// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#ifndef __CONV_AI_PLATFORM_VOLC_PLATFORM_H__
#define __CONV_AI_PLATFORM_VOLC_PLATFORM_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* hal_malloc(size_t size);
void* hal_calloc(size_t num, size_t size);
void* hal_realloc(void* ptr, size_t new_size);
void hal_free(void* ptr);

typedef void* hal_mutex_t;
hal_mutex_t hal_mutex_create(void);
void hal_mutex_lock(hal_mutex_t mutex);
void hal_mutex_unlock(hal_mutex_t mutex);
void hal_mutex_destroy(hal_mutex_t mutex);

uint64_t hal_get_time_ms(void);

int hal_get_uuid(char* uuid, size_t size);

#define THREAD_NAME_MAX_LEN 16
typedef void* hal_tid_t;
typedef struct {
    char name[THREAD_NAME_MAX_LEN];
    int priority;
    int stack_size;
    int bind_cpu;
    int stack_in_ext;
} hal_thread_param_t;

int hal_thread_create(hal_tid_t* thread, const hal_thread_param_t* param, void (*start_routine)(void *), void* args);
int hal_thread_detach(hal_tid_t thread);
void hal_thread_exit(hal_tid_t thread);
void hal_thread_sleep(int time_ms);
void hal_thread_destroy(hal_tid_t thread);

int hal_get_platform_info(char* info, size_t size);

#ifdef __cplusplus
}
#endif
#endif /* __CONV_AI_PLATFORM_VOLC_PLATFORM_H__ */
