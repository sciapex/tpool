#ifndef __THREADPOOL_H
#define __THREADPOOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* a generic thread pool creation routines */
/* 线程池工作队列节点信息 */
typedef struct tpool_work{
    void *(*handler_routine)(void *arg);  /* 调用句柄 */
    void *arg;               /* 句柄参数 */
    struct tpool_work *next; /* 节点指针 */
} tpool_work_t;

/* 线程池信息 */
typedef struct tpool{
    int num_threads;            /* 工作线程数 */
    int max_queue_size;         /* 队列最大值 */
    int do_not_block_when_full; /* 当队列满时不阻塞:1,不阻塞, 0,阻塞*/
    int cur_queue_size;         /* 当前队列大小 */
    int queue_closed;           /* 队列关闭标记 */
    int shutdown;               /* 池关闭标记   */

    pthread_t    *threads;          /* 线程号         */
    tpool_work_t *queue_head;       /* 队列头         */
    tpool_work_t *queue_tail;       /* 队列尾         */
            
    pthread_mutex_t queue_lock;     /* 队列互斥量     */
    pthread_cond_t queue_not_full;  /* 队列未满条件变量 */
    pthread_cond_t queue_not_empty; /* 队列未空条件变量 */
    pthread_cond_t queue_empty;     /* 队列空条件变量 */
} tpool_t;
    
/* private:static */
/* static void tpool_thread(tpool_t *pool); */

/******************************************************************************
 * tpool_init线程池初始化
 * num_worker_threads:线程池线程个数
 * max_queue_size:最大任务数
 * do_not_block_when_full: 任务满时是否阻塞
 * 失败返回NULL
******************************************************************************/
tpool_t *tpool_init(int num_worker_threads,
    int max_queue_size, int do_not_block_when_full);

/*****************************************************************************
 * tpool_add:线程池增加节点
 * pool: 线程池指针
 * routine:工作业务处理
 * 成功返回0, 失败返回-1
 * **************************************************************************/
int tpool_add(tpool_t *pool, void *(*routine)(void *), void *arg);

/******************************************************************************
 * tpool_destroy: 线程池销毁
 * pool: 线程池指针
 * finish: 是否判断队列任务已经都完成
 * 成功返回0, 失败返回-1
 * ***************************************************************************/
int tpool_destroy(tpool_t *pool, int finish);

#ifdef __cplusplus
}
#endif
#endif /* __THREADPOOL_H */