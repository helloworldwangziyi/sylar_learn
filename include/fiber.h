/**
 * @file fiber.h
 * @brief 协程模块
 * @details 基于ucontext_t实现，非对称协程
 * @version 0.1
 * @date 2021-06-15
 */

#ifndef __SYLAR_FIBER_H__
#define __SYLAR_FIBER_H__

#include <functional>
#include <memory>
#include <ucontext.h>
#include "thread.h"

namespace sylar {

/**
 * @brief 协程类
 */
class Fiber : public std::enable_shared_from_this<Fiber> {
public:
    typedef std::shared_ptr<Fiber> ptr;

    /**
     * @brief 协程状态
     * 
     */
    enum State {
        /// 初始化状态
        INIT,
        /// 暂停态，协程执行之前为HOLD状态
        HOLD,
        /// 运行态，协程执行中
        EXEC,
        /// 结束态，协程执行完毕之后为TERM状态
        TERM,
        /// 就绪态，协程执行完毕之后为READY状态
        READY,
        /// 异常态，协程执行过程中出现异常
        EXCEPT
    };

private:
    /**
     * @brief 构造函数
     * @attention 无参构造函数只用于创建线程的第一个协程，也就是线程主函数对应的协程，
     * 这个协程只能由GetThis()方法调用，所以定义成私有方法
     */
    Fiber();

public:
    /**
     * @brief 构造函数，用于创建用户协程
     * @param[] cb 协程入口函数
     * @param[] stacksize 栈大小
     */
    Fiber(std::function<void()> cb, size_t stacksize = 0);

    /**
     * @brief 析构函数
     */
    ~Fiber();

    /**
     * @brief 重置协程状态和入口函数，复用栈空间，不重新创建栈
     * @param[] cb 
     */
    void reset(std::function<void()> cb);

    /**
     * @brief 将当前协程切到到执行状态
     */
    void swapIn();

    /**
     * @brief 当前协程让出执行权
     */
    void swapOut();

    /**
     * @brief 获取协程ID
     */
    uint64_t getId() const { return m_id; }

    /**
     * @brief 获取协程状态
     */
    State getState() const { return m_state; }

public:
    /**
     * @brief 设置当前正在运行的协程，即设置线程局部变量t_fiber的值
     */
    static void SetThis(Fiber *f);

    /**
     * @brief 返回当前线程正在执行的协程
     * @details 如果当前线程还未创建协程，则创建线程的第一个协程，
     * 且该协程为当前线程的主协程，其他协程都通过这个协程来调度，也就是说，其他协程
     * 结束时,都要切回到主协程，由主协程重新选择新的协程进行resume
     * @attention 线程如果要创建协程，那么应该首先执行一下Fiber::GetThis()操作，以初始化主函数协程
     */
    static Fiber::ptr GetThis();

    /**
     * @brief 协程切换到后台，并设置为Ready状态
    */
    static void YieldToReady();

    /**
     * @brief 协程切换到后台，并设置为Hold状态
    */
    static void YieldToHold();

    /**
     * @brief 获取总协程数
     */
    static uint64_t TotalFibers();

    /**
     * @brief 协程入口函数
     */
    static void MainFunc();

    /**
     * @brief 获取当前协程id
     */
    static uint64_t GetFiberId();

private:
    /// 协程id
    uint64_t m_id        = 0;
    /// 协程栈大小
    uint32_t m_stacksize = 0;
    /// 协程状态
    State m_state        = INIT;
    /// 协程上下文
    ucontext_t m_ctx;
    /// 协程栈地址
    void *m_stack = nullptr;
    /// 协程入口函数
    std::function<void()> m_cb;
};

} // namespace sylar

#endif