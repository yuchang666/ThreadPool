#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
#include "ThreadPool.h"

ThreadPool *ThreadPool::self_ = nullptr;

struct worker;
struct ThreadPoolImpl;

struct ThreadPoolImpl
{
    size_t maxThreadsNum_;
    std::mutex queueMutex_;
    std::mutex cvMutex_;
    std::mutex threadMutex_;
    std::condition_variable taskCv_;
    std::multimap<ThreadPool::PRIORITY, ThreadPool::task> taskQueue_;
    std::vector<worker> threadVec_;
    ThreadPoolImpl();
    ~ThreadPoolImpl();
};

struct worker
{
    bool on_;
    std::thread t_;
    ThreadPoolImpl *impl_;
    worker(ThreadPoolImpl *impl);
    worker(worker &&other);
    ~worker();
    void workerThread();
};

ThreadPoolImpl::ThreadPoolImpl()
{
    maxThreadsNum_ = 1;
}
ThreadPoolImpl::~ThreadPoolImpl()
{
}

worker::worker(ThreadPoolImpl *impl)
{
    on_ = true;
    impl_ = impl;

    t_ = std::thread(&worker::workerThread, this);
}

worker::worker(worker &&other) : impl_(other.impl_), t_(static_cast<std::thread &&>(other.t_))
{
}

worker::~worker()
{
    if (t_.joinable())
    {
        t_.join();
    }
}

void worker::workerThread()
{
    while (on_)
    {
        if (impl_->taskQueue_.empty())
        {
            if (impl_->threadVec_.size() > MAX_THREADS_NO_TASK)
            {
                std::unique_lock<std::mutex> threadlk(impl_->threadMutex_);
                impl_->threadVec_.back().on_ = false;
                impl_->threadVec_.pop_back();
            }
            std::unique_lock<std::mutex> cvlk(impl_->cvMutex_);
            impl_->taskCv_.wait(cvlk);
        }

        {
            std::unique_lock<std::mutex> lk2(impl_->queueMutex_);
            auto tsk = std::move(impl_->taskQueue_.begin()->second);
            impl_->taskQueue_.erase(impl_->taskQueue_.begin());
            lk2.unlock();
            tsk();
        }

        {
            std::unique_lock<std::mutex> threadlk(impl_->threadMutex_);
            if (impl_->taskQueue_.size() > MAX_TASK_IN_QUE && impl_->threadVec_.size() < impl_->maxThreadsNum_)
            {
                impl_->threadVec_.emplace_back(impl_);
            }
        }
    }
}

ThreadPool::ThreadPool()
{
    impl_ = std::make_unique<ThreadPoolImpl>();
    impl_->threadVec_.emplace_back(impl_.get());
}

ThreadPool *ThreadPool::instance()
{
    if (self_ == nullptr)
    {
        self_ = new ThreadPool;
    }
    return self_;
}

ThreadPool::~ThreadPool()
{
    delete this;
}

void ThreadPool::setMaxThreadsNum(size_t size)
{
    while (size < impl_->threadVec_.size())
    {
        std::unique_lock<std::mutex> threadlk(impl_->threadMutex_);
        impl_->threadVec_.back().on_ = false;
        impl_->threadVec_.pop_back();
    }
    impl_->maxThreadsNum_ = size;
}

void ThreadPool::run(task t, PRIORITY pri)
{
    std::unique_lock<std::mutex> lk(impl_->queueMutex_);
    std::unique_lock<std::mutex> cvlk(impl_->cvMutex_);
    impl_->taskCv_.notify_all();
    impl_->taskQueue_.insert(std::make_pair(pri, std::move(t)));
}

void ThreadPool::exit()
{
    for (auto &t : impl_->threadVec_)
    {
        t.on_ = false;
    }
}
