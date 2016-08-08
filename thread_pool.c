#include "thread_pool.h"

/* Private:the worker thread */
static void *tpool_thread(void *tpool);

tpool_t *tpool_init(int num_worker_threads,
        int max_queue_size, int do_not_block_when_full)
{
    int i = 0;
    int rtn = 0;
    tpool_t *pool = NULL;

    /* init pool  begin ... */
    /* make the thread pool structure */
    if((pool = (struct tpool *)malloc(sizeof(struct tpool))) == NULL) {
        //lprintf(log, FATAL, "Unable to malloc() thread pool!\n");
        return NULL;
    }

    /* set the desired thread pool values:num_threads < max_queue_size */
    pool->num_threads = num_worker_threads;                      /*工作线程数*/
    pool->max_queue_size = max_queue_size;                       /*任务链表最大长度*/
    pool->do_not_block_when_full = do_not_block_when_full;       /*任务链表满时是否等待*/

    /* create an array to hold a ptr to the worker threads   生成线程池缓存
     * pthread_t threads[num_worker_threads];*/
    if((pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_worker_threads)) == NULL) {
        //lprintf(log, FATAL,"Unable to malloc() thread info array\n");
        return NULL;
    }

    /* initialize the work queue  初始化任务链表 */
    pool->cur_queue_size = 0;
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->queue_closed = 0;
    pool->shutdown = 0;

    /* create the mutexs and cond vars  初始化互斥变量 条件变量 用于线程之间的同步 */
    if((rtn = pthread_mutex_init(&(pool->queue_lock), NULL)) != 0) {
        //lprintf(log,FATAL,"pthread_mutex_init %s",strerror(rtn));
        return NULL;
    }

    if((rtn = pthread_cond_init(&(pool->queue_not_empty), NULL)) != 0) {
        //lprintf(log,FATAL,"pthread_cond_init %s",strerror(rtn));
        return NULL;
    }

    if((rtn = pthread_cond_init(&(pool->queue_not_full), NULL)) != 0) {
        //lprintf(log,FATAL,"pthread_cond_init %s",strerror(rtn));
        return NULL;
    }

    if((rtn = pthread_cond_init(&(pool->queue_empty), NULL)) != 0) {
        //lprintf(log,FATAL,"pthread_cond_init %s",strerror(rtn));
        return NULL;
    }

    /* 
     * from "man 3c pthread_attr_init"
     * Define the scheduling contention scope for the created thread.  The only
     * value     supported    in    the    LinuxThreads    implementation    is
     * !PTHREAD_SCOPE_SYSTEM!, meaning that the threads contend  for  CPU  time
     * with all processes running on the machine.
     *
     * so no need to explicitly set the SCOPE
     */

    /* create the individual worker threads */
    for(i = 0; i != num_worker_threads; ++i) {
        if( (rtn=pthread_create(&(pool->threads[i]), NULL,
            tpool_thread, (void*)pool)) != 0) {
            //lprintf(log,FATAL,"pthread_create %s\n",strerror(rtn));
            return NULL;
        }

        //lprintf(log, INFO, "init pthread  %d!\n",i);
    }
    //lprintf(log, INFO, "init pool end!\n");
    return pool;
}

int tpool_destroy(tpool_t *pool, int finish)
{
    int i, rtn;
    tpool_work_t *cur;   /*当前工作线程句柄*/

    /* destroy pool begin! */
    if(pool == NULL) {
        return 0;
    }
    
    /* relinquish control of the queue */
    if((rtn = pthread_mutex_lock(&(pool->queue_lock))) != 0) {
        //lprintf(log,FATAL,"pthread mutex lock failure\n");
        return -1;
    }

    /* is a shutdown already going on ? */
    if(pool->queue_closed || pool->shutdown) {
        if((rtn = pthread_mutex_unlock(&(pool->queue_lock))) != 0) {
            //lprintf(log,FATAL,"pthread mutex lock failure\n");
            return -1;
        }
        return 0;
    }

    //lprintf(log, INFO, "destroy pool begin 2!\n");
    /* close the queue to any new work   对于新任务来说所有交易禁止 */
    pool->queue_closed = 1;

    /* if the finish flag is set, drain the queue */
    /* finish标识打开, 则需要等到队列任务都已完成才能继续执行销毁 */
    if(finish) {
        while(pool->cur_queue_size != 0) {
            /* wait for the queue to become empty,
             * while waiting queue lock will be released */
            if((rtn = pthread_cond_wait(&(pool->queue_empty),
                            &(pool->queue_lock))) != 0) {
                //lprintf(log,FATAL,"pthread_cond_wait %d\n",rtn);
                return -1;
            }
        }
    }

    //lprintf(log, INFO, "destroy pool begin 3!\n");
    /* set the shutdown flag */
    pool->shutdown = 1;

    if((rtn = pthread_mutex_unlock(&(pool->queue_lock))) != 0) {
        //lprintf(log,FATAL,"pthread mutex unlock failure\n");
        return -1;
    }
    /*  return 0; */

    //lprintf(log, INFO, "destroy pool begin 4!\n");

    /* wake up all workers to rechedk the shutdown flag */
    if((rtn = pthread_cond_broadcast(&(pool->queue_not_empty)))
            != 0) {
        //lprintf(log,FATAL,"pthread_cond_boradcast %d\n",rtn);
        return -1;
    }

    if((rtn = pthread_cond_broadcast(&(pool->queue_not_full)))
            != 0) {
        //lprintf(log,FATAL,"pthread_cond_boradcast %d\n",rtn);
        return -1;
    }

    /* wait for workers to exit */
    for(i = 0; i < pool->num_threads; i++) {
        if((rtn = pthread_join(pool->threads[i],NULL)) != 0) {
            //lprintf(log,FATAL,"pthread_join %d\n",rtn);
            return -1;
        }
    }

    /* clean up memory */
    free(pool->threads);
    while(pool->queue_head != NULL) {
        //cur = pool->queue_head->next; error
        cur = pool->queue_head;
        pool->queue_head = pool->queue_head->next;
        free(cur);
    }
    free(pool);
    pool = NULL;

    //lprintf(log, INFO, "destroy pool end!\n");
    return 0;
}

int tpool_add(tpool_t *pool, void *(*routine)(void *), void *arg)
{
    int rtn = 0;
    tpool_work_t *workp = NULL;

    pthread_mutex_lock(&pool->queue_lock);

    /* now we have exclusive access to the work queue ! */
    if((pool->cur_queue_size == pool->max_queue_size) &&
            (pool->do_not_block_when_full)) {
        pthread_mutex_unlock(&pool->queue_lock);
        return -2;
    }

    /* wait for the queue to have an open space for new work, while
     *      * waiting the queue_lock will be released */
    while((pool->cur_queue_size == pool->max_queue_size) &&
            (!(pool->shutdown))) {
        pthread_cond_wait(&(pool->queue_not_full), &(pool->queue_lock));
    }

    if(pool->shutdown) {
        pthread_mutex_unlock(&pool->queue_lock);
        return -3;
    }

    /* allocate the work structure */
    if((workp = (tpool_work_t *)malloc(sizeof(tpool_work_t)))
            == NULL) {
        //lprintf(log,FATAL,"unable to create work struct\n");
        return -1;
    }

    /* set the function/routine which will handle the work,
     *      * (note: it must be reenterant) */
    workp->handler_routine = routine;
    workp->arg = arg;
    workp->next = NULL;

    if(pool->cur_queue_size == 0) {
        pool->queue_tail = pool->queue_head = workp;
        if((rtn = pthread_cond_broadcast(&(pool->queue_not_empty))) != 0) {
            //lprintf(log,FATAL,"pthread broadcast error\n");
            return -1;
        }
    } else {
        pool->queue_tail->next = workp;
        pool->queue_tail = workp;
    }


    pool->cur_queue_size++;

    /* relinquish control of the queue */
    pthread_mutex_unlock(&pool->queue_lock);

    return 0;
}

/*****************************************
 * 内部线程执行函数          
 * 每个线程实际执行(*(my_work->handler_routine))(my_work->arg)
 *****************************************/
void *tpool_thread(void *tpool)
{
    tpool_work_t *my_work;
    tpool_t *pool = (struct tpool *)tpool;

 
    for(;;) /* go forever */
    {

        pthread_mutex_lock(&(pool->queue_lock));

        /* sleep until there is work,
         * while asleep the queue_lock is relinquished */
        while((pool->cur_queue_size == 0) && (!pool->shutdown)) { /* 任务列表为0  并且 线程池没有关闭 */
            pthread_cond_wait(&(pool->queue_not_empty),           /* 等待直到，任务到来为止 */
                    &(pool->queue_lock));
        }

        /* are we shutting down ?  线程池是否已经关闭，如果线程池关闭则线程自己主动关闭 */
        if(pool->shutdown) {
            pthread_mutex_unlock(&(pool->queue_lock));
            pthread_exit(NULL);      /*线程退出状态为空，主线程不捕获各副线程状态*/
        }

        /* process the work: 从队列头开始 */
        my_work = pool->queue_head;
        pool->cur_queue_size--;

        /*将任务链表头部去掉，改任务正在处理中*/
        if(pool->cur_queue_size == 0)
            pool->queue_head = pool->queue_tail = NULL;
        else
            pool->queue_head = my_work->next;

        /* broadcast that the queue is not full */
        if((!pool->do_not_block_when_full) &&
                (pool->cur_queue_size == (pool->max_queue_size - 1))) {
            pthread_cond_broadcast(&(pool->queue_not_full));
        }

        /*任务链表为空*/
        if(pool->cur_queue_size == 0) {
            pthread_cond_signal(&(pool->queue_empty));
        }

        pthread_mutex_unlock(&(pool->queue_lock));

        /* soket = (int*)my_work->arg;  */
        /* perform the work */
        /*启动线程业务处理逻辑*/
        (*(my_work->handler_routine))(my_work->arg);
        free(my_work);
    }

    return(NULL);
}