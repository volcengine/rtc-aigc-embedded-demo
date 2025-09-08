// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#include "volc_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>

void* hal_malloc(size_t size) {
    return heap_caps_malloc(size,MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
}

void* hal_calloc(size_t num, size_t size) {
    return heap_caps_calloc(num,size,MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
}

void* hal_realloc(void* ptr, size_t new_size) {
    return heap_caps_realloc(ptr,new_size,MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
}

void hal_free(void* ptr) {
    heap_caps_free(ptr);
}

hal_mutex_t hal_mutex_create(void) {
    pthread_mutex_t* p_mutex = NULL;
    pthread_mutexattr_t attr;

    p_mutex = (pthread_mutex_t *)hal_calloc(1, sizeof(pthread_mutex_t));
    if (NULL == p_mutex) {
        return NULL;
    }

    if (0 != pthread_mutexattr_init(&attr) ||
        0 != pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL) ||
        0 != pthread_mutex_init(p_mutex, &attr)) {
        hal_free(p_mutex);
        return NULL;
    }
    
    return (hal_mutex_t)p_mutex;
}
void hal_mutex_lock(hal_mutex_t mutex) {
    pthread_mutex_lock((pthread_mutex_t *)mutex);
}

void hal_mutex_unlock(hal_mutex_t mutex) {
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

void hal_mutex_destroy(hal_mutex_t mutex) {
    pthread_mutex_t* p_mutex = (pthread_mutex_t *)mutex;
    if (NULL == p_mutex) {
        return;
    }
    pthread_mutex_destroy(p_mutex);
    hal_free(p_mutex);
}

uint64_t hal_get_time_ms(void) {
    struct timespec now_time;
    clock_gettime(CLOCK_REALTIME, &now_time);
    return (uint64_t)now_time.tv_sec * 1000 + (uint64_t)now_time.tv_nsec / 1000000;
}

int hal_get_uuid(char* uuid, size_t size) {
    esp_netif_t *netif = NULL;
    
    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        uint8_t hwaddr[6] = {0};
        esp_netif_get_mac(netif, hwaddr);
        snprintf(uuid, size,"%02X%02X%02X%02X%02X%02X",hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
    } 
    return 0;
}

int hal_thread_create(hal_tid_t* thread, const hal_thread_param_t* param, void (*start_routine)(void *), void* args) {
    int ret = 0;
    int stack_size = 0;
    int priority = 0;
    BaseType_t core_id = 0;
    TaskHandle_t* handle = NULL;
    if (NULL == thread || NULL == start_routine) {
        return -1;
    }

    if (NULL != param) {
        stack_size = param->stack_size <= 0 ? 8192 : param->stack_size;
        priority = param->priority <= 0 ? 3 : param->priority;
        core_id = 1;
    } else {
        stack_size = 8192;
        priority = 3;
        core_id = tskNO_AFFINITY;
    }
    handle = (TaskHandle_t *)hal_calloc(1, sizeof(TaskHandle_t));
    if (NULL == handle) {
        return -1;
    }
    *thread = (hal_tid_t *)handle;

    if(param->stack_in_ext) {
        ret = xTaskCreatePinnedToCoreWithCaps(start_routine, param->name, stack_size, args, priority, handle, core_id, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    } else {
        ret = xTaskCreatePinnedToCore(start_routine, param->name, stack_size, args, priority, handle, core_id);
    }
    if (pdPASS != ret) {
        return -1;
    }
    return 0;
}
int hal_thread_detach(hal_tid_t thread) {
    return pthread_detach((pthread_t)thread);
}

void hal_thread_exit(hal_tid_t thread) {
    vTaskDelete(NULL);
}

void hal_thread_sleep(int time_ms) {
    vTaskDelay(pdMS_TO_TICKS(time_ms));
}

void hal_thread_destroy(hal_tid_t thread) {
    if (NULL == thread) {
        return;
    }
    hal_free(thread);
}

int hal_get_platform_info(char* info, size_t size) {
    if (NULL == info || size <= 0) {
        return -1;
    }
    snprintf(info, size, "esp32");
    return 0;
}
