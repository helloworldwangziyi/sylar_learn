# 协程调度
这个应该是整个模块中最难理解的部分了，首先你必须了解之前写的协程模块的工作原理，这样你才有可能理解这个调度器的工作原理，如果你无法理解上下文切换，那么我可以保证你在按照视频做的时候会很难理解。

准备好了，开始吧！
首先，我们需要明确一个蓝图，我们main函数创建主线程，然后通过scheduler创建调度器，（也就是说主线程创建调度器），由调度器的构造函数的use_caller参数来决定我们是否复用主线程（就是将主线程也当作调度线程），之后scheduler通过start方法创建子线程（子线程的回调函数就是我们的调度程序），然后我们将任务协程通过scheduler方法添加到list中，通知线程来取任务执行上下文的切换。这就是协程调度的大致流程。

## use_caller = true
use_caller 这是啥？简单来说当这个变量被置为true时，我们就会将主线程（也就是main函数线程）也纳入到调度协程中参与调度，这样做的好处显而易见就是我们节省了一个线程，但是害处也很明显就是我们这个主线程不能像子线程那样只执行一个调度任务，所以我们有必要分出一个主调度协程来（代替子线程的回调函数）辅助我们的主线程进行调度。

所以我们的调度器构造函数在use_caller为true时这么写
```c++
Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    :m_name(name) {
    SYLAR_ASSERT(threads > 0);

    if(use_caller) {
        sylar::Fiber::GetThis();
        --threads;

        SYLAR_ASSERT(GetThis() == nullptr);
        t_scheduler = this;

        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
        sylar::Thread::SetName(m_name);

        t_fiber = m_rootFiber.get();
        m_rootThread = sylar::GetThreadId();
        m_threadIds.push_back(m_rootThread);
```
首先我们会创建主协程（一定要区分主协程和主调度协程的区别）在之前协程的章节中，我们知道需要一个主协程来和任务协程进行切换，使得任务协程在执行完任务之后可以正常的回到调用协程线程的上下文环境中，这里的主协程还是这个作用，但是他负责和主调度协程进行切换，而主调度协程负责和任务协程进行切换，这样才能保证程序的正常运行。（这一段非常重要一定要理解，否则调试的时候协程切换来切换去你就不知道那是那了）

m_rootFiber就是主调度协程，执行的方法就是Scheduler::run方法，这个方法是一个静态方法，后面会介绍。

之后就是设置设置线程名称，获取线程id加入线程id列表。

## start 方法

```c++
void Scheduler::start() {
    MutexType::Lock lock(m_mutex);
    if(!m_stopping) {
        return;
    }
    m_stopping = false;
    SYLAR_ASSERT(m_threads.empty());

    m_threads.resize(m_threadCount);
    for(size_t i = 0; i < m_threadCount; ++i) {
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this)
                            , m_name + "_" + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }
    lock.unlock();

    if(m_rootFiber) {
       //m_rootFiber->swapIn();
       m_rootFiber->call();
       SYLAR_LOG_INFO(g_logger) << "call out " << m_rootFiber->getState();
    }
}
```
上面所见的就是第一版的start方法，我们start就会马上创建相应数量的线程，线程的回调函数设置为Scheduler::run,是不是有点眼熟，就是最开始use_caller为true时我们设置主调度协程的函数，这里我们就应该明白了为啥use_caller会使我们的程序编写变得麻烦，因为我们主线程中运行这调度器实例，而又不能像子线程那样只执行一个任务，所以我们需要一个主调度协程来辅助我们的主线程进行调度，这样我们的主线程就可以正常的执行其他任务了。有人问use_caller设置为false会怎样，那么就不会再有主调度协程了，只会有调度线程，这样理解起来就方便一些。(这里start的实现还是有一点小bug
后面会说)

调度线程和主调度协程都会在第一时间执行run方法，我们就看看run怎么写的
```c++
void Scheduler::run() {
    SYLAR_LOG_INFO(g_logger) << "run";
    setThis();
    if(sylar::GetThreadId() != m_rootThread) {
        t_fiber = Fiber::GetThis().get();
    }

    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
    Fiber::ptr cb_fiber;

    FiberAndThread ft;
    while(true) {
        ft.reset();
        bool tickle_me = false;
        bool is_active = false;
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_fibers.begin();
            while(it != m_fibers.end()) {
                if(it->thread != -1 && it->thread != sylar::GetThreadId()) {
                    ++it;
                    tickle_me = true;
                    continue;
                }

                SYLAR_ASSERT(it->fiber || it->cb);
                if(it->fiber && it->fiber->getState() == Fiber::EXEC) {
                    ++it;
                    continue;
                }

                ft = *it;
                m_fibers.erase(it);
                ++m_activeThreadCount;
                is_active = true;
                break;
            }
        }

        if(tickle_me) {
            tickle();
        }

        if(ft.fiber && (ft.fiber->getState() != Fiber::TERM
                        && ft.fiber->getState() != Fiber::EXCEPT)) {
            ft.fiber->swapIn();
            --m_activeThreadCount;

            if(ft.fiber->getState() == Fiber::READY) {
                schedule(ft.fiber);
            } else if(ft.fiber->getState() != Fiber::TERM
                    && ft.fiber->getState() != Fiber::EXCEPT) {
                ft.fiber->m_state = Fiber::HOLD;
            }
            ft.reset();
        } else if(ft.cb) {
            if(cb_fiber) {
                cb_fiber->reset(ft.cb);
            } else {
                cb_fiber.reset(new Fiber(ft.cb));
            }
            ft.reset();
            cb_fiber->swapIn();
            --m_activeThreadCount;
            if(cb_fiber->getState() == Fiber::READY) {
                schedule(cb_fiber);
                cb_fiber.reset();
            } else if(cb_fiber->getState() == Fiber::EXCEPT
                    || cb_fiber->getState() == Fiber::TERM) {
                cb_fiber->reset(nullptr);
            } else {//if(cb_fiber->getState() != Fiber::TERM) {
                cb_fiber->m_state = Fiber::HOLD;
                cb_fiber.reset();
            }
        } else {
            if(is_active) {
                --m_activeThreadCount;
                continue;
            }
            if(idle_fiber->getState() == Fiber::TERM) {
                SYLAR_LOG_INFO(g_logger) << "idle fiber term";
                break;
            }

            ++m_idleThreadCount;
            idle_fiber->swapIn();
            --m_idleThreadCount;
            if(idle_fiber->getState() != Fiber::TERM
                    && idle_fiber->getState() != Fiber::EXCEPT) {
                idle_fiber->m_state = Fiber::HOLD;
            }
        }
    }
}
```
这么长，别害怕慢慢来，首先需要设置当前线程局部变量为调度器实例，这一步通过setThis();来完成，这样我们的t_scheduler参数就被赋值为当前调度器实例，通过这种方式我们就可以让每一个线程和主调度协程获取一个自己的调度器实例（这个是线程局部变量的特性，即每一个线程都会获取一个副本，这个副本两两之间互不干扰），也就是说每个线程的线程调度器都是他们独有的。

接下来，如果当前线程不是根线程，我们就需要在当前线程中创建主协程，目的就是为了更好的进行切换，因为子线程中没有主调度协程，所以我们要实现线程调度就必须有一个主线程（如果这里没有看明白就需要回头理解一下什么是主线程什么是主调度协程了）。

再接下来我们创建两个协程，一个用来在空闲时运行，另一个用来在只传入回调函数时，用传入的回调函数来创建协程。

接下来就是协程调度的主要逻辑，首先我们尝试获取协程，有的协程比较挑剔它指定了要在具体的线程上运行，所以我们需要判断一下当前线程是否是指定的线程，如果不是，我们就需要将这个协程放到调度队列中，等待指定的线程来调度它，这里我们就需要用到tickle方法了，tickle方法的作用就是唤醒指定线程的调度器，让它来调度这个协程。

好的我们终于获取到了任务协程，现在就要开始执行了，我们需要区别一下swapIn 和 call 这两方法，call是将当前协程和主协程进行切换，而swapIn则是分两种情况，在主线程中swapIn是将主调度协程和主协程进行切换，但是在子线程中就是直接将当前协程和主协程进行切换（这里很绕，但是就是这样，如果搞错的话进程就跑飞了）， 接下来的逻辑就不难了，简单来说就是如果传入的是回调函数，那么就创建一个协程来执行，所有的任务执行完成就执行空闲协程，最后空闲线程也执行完了(空闲协程会一直和主协程切换，并且设置自己的状态为HOLD，以便让出执行权，空闲协程会在stop方法中停止，因为只有在那里stopping方法才会被设置为true，从而从idle协程中切换出来，注意只要m_acctiveCount不为0那么就会一直执行idle)，那么run方法就break掉了。但是空闲协程不是那么容易执行完的，我们看看定义
```c++
void Scheduler::idle() {
    SYLAR_LOG_INFO(g_logger) << "idle";
    while(!stopping()) {
        sylar::Fiber::YieldToHold();
    }
}
```
stopping 只有返回true才会退出循环，而stopping的定义如下
```c++
bool Scheduler::stopping() {
    MutexType::Lock lock(m_mutex);
    return m_autoStop && m_stopping
        && m_fibers.empty() && m_activeThreadCount == 0;
}
```
然后我们再看看最终的stop
```c++
void Scheduler::stop() {
    m_autoStop = true;
    if(m_rootFiber
            && m_threadCount == 0
            && (m_rootFiber->getState() == Fiber::TERM
                || m_rootFiber->getState() == Fiber::INIT)) {
        SYLAR_LOG_INFO(g_logger) << this << " stopped";
        m_stopping = true;

        if(stopping()) {
            return;
        }
    }

    //bool exit_on_this_fiber = false;
    if(m_rootThread != -1) {
        SYLAR_ASSERT(GetThis() == this);
    } else {
        SYLAR_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    for(size_t i = 0; i < m_threadCount; ++i) {
        tickle();
    }

    if(m_rootFiber) {
        tickle();
    }

    if(m_rootFiber) {
        //while(!stopping()) {
        //    if(m_rootFiber->getState() == Fiber::TERM
        //            || m_rootFiber->getState() == Fiber::EXCEPT) {
        //        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
        //        SYLAR_LOG_INFO(g_logger) << " root fiber is term, reset";
        //        t_fiber = m_rootFiber.get();
        //    }
        //    m_rootFiber->call();
        //}
        if(!stopping()) {
            m_rootFiber->call();
        }
    }

    std::vector<Thread::ptr> thrs;
    {
        MutexType::Lock lock(m_mutex);
        thrs.swap(m_threads);
    }

    for(auto& i : thrs) {
        i->join();
    }
    //if(exit_on_this_fiber) {
    //}
}
```
这里还是要对有无主调度协程进行区分，如果有主调度协程，且满足状态为TREM EXECPT ，线程数量为0 那么就尝试直接退出，否则就继续看看是不是还有线程没有执行完，或者还有正在运行的协程，我们会唤醒所有线程和主调度协程，将剩下的任务执行完毕，最后再将所有线程join掉，这样就完成了协程调度的停止。为啥要在stop里面唤醒主调度线程呢？这个其实是为了防止你在start方法执行的时候没有往任务list中添加执行任务，那么就会自动退出，这样你就永远不会执行list的任务了。这个仅针对你现在只创建了一个线程并设置use_caller为true.(但是我又测试了一下，发现这个重新运行主调度协程的方法可以写在start中，因为我们后面将idle改为一直切换而不是直接退出)。

好的基本上能遇到的坑都填上了。




