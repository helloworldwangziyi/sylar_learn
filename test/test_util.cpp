/**
 * @file example.c
 * @author your name (you@domain.com)
 * @brief ucontext_t示例函数，演示基于ucontext_t的上下文切换
 * @version 0.1
 * @date 2021-07-02
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "sylar.h"
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

/*
// 上下文结构体定义
// 这个结构体是平台相关的，因为不同平台的寄存器不一样
// 下面列出的是所有平台都至少会包含的4个成员
typedef struct ucontext_t {
    // 当前上下文结束后，下一个激活的上下文对象的指针，只在当前上下文是由makecontext创建时有效
    struct ucontext_t *uc_link;
    // 当前上下文的信号屏蔽掩码
    sigset_t          uc_sigmask;
    // 当前上下文使用的栈内存空间，只在当前上下文是由makecontext创建时有效
    stack_t           uc_stack;
    // 平台相关的上下文具体内容，包含寄存器的值
    mcontext_t        uc_mcontext;
    ...
} ucontext_t;
 
// 获取当前的上下文
int getcontext(ucontext_t *ucp);
 
// 恢复ucp指向的上下文，这个函数不会返回，而是会跳转到ucp上下文对应的函数中执行，相当于变相调用了函数
int setcontext(const ucontext_t *ucp);
 
// 修改由getcontext获取到的上下文指针ucp，将其与一个函数func进行绑定，支持指定func运行时的参数，
// 在调用makecontext之前，必须手动给ucp分配一段内存空间，存储在ucp->uc_stack中，这段内存空间将作为func函数运行时的栈空间，
// 同时也可以指定ucp->uc_link，表示函数运行结束后恢复uc_link指向的上下文，
// 如果不赋值uc_link，那func函数结束时必须调用setcontext或swapcontext以重新指定一个有效的上下文，否则程序就跑飞了
// makecontext执行完后，ucp就与函数func绑定了，调用setcontext或swapcontext激活ucp时，func就会被运行
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);
 
// 恢复ucp指向的上下文，同时将当前的上下文存储到oucp中，
// 和setcontext一样，swapcontext也不会返回，而是会跳转到ucp上下文对应的函数中执行，相当于调用了函数
// swapcontext是sylar非对称协程实现的关键，线程主协程和子协程用这个接口进行上下文切换
int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);
*/

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_assert() {
    SYLAR_LOG_INFO(g_logger) << sylar::BacktraceToString(10);
    SYLAR_ASSERT2(0 == 1, "abcdef xx");
}

static ucontext_t uctx_main, uctx_func1, uctx_func2;

#define handle_error(msg)   \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

static void func1(void) {
    printf("func1: started\n");
    printf("func1: swapcontex(&uctx_func1, &uctx_func2)\n");
    // 激活uctx_func2上下文，从func1返回func2的swapcontext下一行开始执行
    if (swapcontext(&uctx_func1, &uctx_func2) == -1)
        handle_error("swapcontext");
    // func2结束，uctx_func1被激活，func1继续执行下面的语句
    printf("func1:returning\n");
    // func1结束，激活uctx_main，返回main函数中继续执行
}

static void func2(void) {
    printf("func2: started\n");
    printf("func2: swapcontex(&uctx_func2, &uctx_func1)\n");
    // 激活uctx_func1上下文，相当于跳转到func1中执行
    if (swapcontext(&uctx_func2, &uctx_func1) == -1)
        handle_error("swapcontext");
    printf("func2:returning\n");
    // func2结束时会恢复uctx_func1的上下文，相当于func2结束后会继续执行func1
}

int main(int argc, char *argv[]) {
    char func1_stack[16384];
    char func2_stack[16384];

    if (getcontext(&uctx_func1) == -1)
        handle_error("getcontext");

    // uctx_func1作为func1执行时的上下文，其返回时恢复uctx_main指定的上下文
    uctx_func1.uc_link          = &uctx_main;
    uctx_func1.uc_stack.ss_sp   = func1_stack;
    uctx_func1.uc_stack.ss_size = sizeof(func1_stack);
    makecontext(&uctx_func1, func1, 0); 

    if (getcontext(&uctx_func2) == -1)
        handle_error("getcontext");

    // uctx_func2作为func2执行时的上下文，其返回时恢复uctx_func1指定的上下文
    uctx_func2.uc_link          = (argc > 1) ? NULL : &uctx_func1;
    uctx_func2.uc_stack.ss_sp   = func2_stack;
    uctx_func2.uc_stack.ss_size = sizeof(func2_stack);
    makecontext( &uctx_func2, func2, 0); 

    // 激活uctx_func2指定的上下文，同时将旧的当前上下文，即原main函数的上下文保存在uctx_main里
    // swapcontext之后将跳转到func2函数中执行
    printf("main: swapcontext(&uctx_main, &uctx_func2)\n");
    if (swapcontext(&uctx_main, &uctx_func2) == -1)
        handle_error("swapcontext");

    // func1结束，uctx_main被激活，main函数继续运行
    printf("main: exiting\n");
    exit(EXIT_SUCCESS);
}