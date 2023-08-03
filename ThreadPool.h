#ifndef THREADPOOL
#define THREADPOOL
/*ThreadPool of singleton*/
#include <iostream>
#include <functional>
#include <memory>

struct ThreadPoolImpl;

/*increase threads number when the size of task queue is larger than this number*/
#define MAX_TASK_IN_QUE 10
/*decre threads number when the task queue is empty and  threads number is larger than this number*/
#define MAX_THREADS_NO_TASK 1

class ThreadPool
{
public:
    using task = std::function<void()>;

    // priority
    enum PRIORITY : unsigned char
    {
        SERIOUS = 0,
        URGENT = 1,
        NORMAL = 2,

    };
    static ThreadPool *self_;

public:
    static ThreadPool *instance();
    ~ThreadPool();

    /*set max threads number*/
    void setMaxThreadsNum(size_t size);

    /*add task to queue*/
    void run(task t, PRIORITY pri = PRIORITY::NORMAL);

    /*stop thread pool*/
    void exit();

private:
    ThreadPool();

private:
    std::unique_ptr<ThreadPoolImpl> impl_;
};

#endif /* THREADPOOL */
