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
/* �̳߳ع������нڵ���Ϣ */
typedef struct tpool_work{
    void *(*handler_routine)(void *arg);  /* ���þ�� */
    void *arg;               /* ������� */
    struct tpool_work *next; /* �ڵ�ָ�� */
} tpool_work_t;

/* �̳߳���Ϣ */
typedef struct tpool{
    int num_threads;            /* �����߳��� */
    int max_queue_size;         /* �������ֵ */
    int do_not_block_when_full; /* ��������ʱ������:1,������, 0,����*/
    int cur_queue_size;         /* ��ǰ���д�С */
    int queue_closed;           /* ���йرձ�� */
    int shutdown;               /* �عرձ��   */

    pthread_t    *threads;          /* �̺߳�         */
    tpool_work_t *queue_head;       /* ����ͷ         */
    tpool_work_t *queue_tail;       /* ����β         */
            
    pthread_mutex_t queue_lock;     /* ���л�����     */
    pthread_cond_t queue_not_full;  /* ����δ���������� */
    pthread_cond_t queue_not_empty; /* ����δ���������� */
    pthread_cond_t queue_empty;     /* ���п��������� */
} tpool_t;
    
/* private:static */
/* static void tpool_thread(tpool_t *pool); */

/******************************************************************************
 * tpool_init�̳߳س�ʼ��
 * num_worker_threads:�̳߳��̸߳���
 * max_queue_size:���������
 * do_not_block_when_full: ������ʱ�Ƿ�����
 * ʧ�ܷ���NULL
******************************************************************************/
tpool_t *tpool_init(int num_worker_threads,
    int max_queue_size, int do_not_block_when_full);

/*****************************************************************************
 * tpool_add:�̳߳����ӽڵ�
 * pool: �̳߳�ָ��
 * routine:����ҵ����
 * �ɹ�����0, ʧ�ܷ���-1
 * **************************************************************************/
int tpool_add(tpool_t *pool, void *(*routine)(void *), void *arg);

/******************************************************************************
 * tpool_destroy: �̳߳�����
 * pool: �̳߳�ָ��
 * finish: �Ƿ��ж϶��������Ѿ������
 * �ɹ�����0, ʧ�ܷ���-1
 * ***************************************************************************/
int tpool_destroy(tpool_t *pool, int finish);

#ifdef __cplusplus
}
#endif
#endif /* __THREADPOOL_H */