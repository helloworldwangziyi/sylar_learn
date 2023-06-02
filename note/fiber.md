# 协程（纤程）模块
协程是更轻量级的线程，线程和进程都是需要通过操作系统来进行调度的，而协程是由用户态来进行调度的，所以协程的切换比线程的切换更加轻量级，所以协程的性能比线程的性能更好。（这是关于协程的介绍）

既然是用户态进行调度，我们就需要知道如何进行调度，调度有需要依赖上下文的切换，所以我们就需要知道上下文是啥，怎么使用。

## 上下文（简介）
不研究复杂的汇编，上下文通俗的来说就是函数的栈帧，我们知道函数的栈帧是由函数的参数，返回值，局部变量，返回地址，上一个函数的栈帧指针组成的，所以我们就可以通过这些来进行上下文的切换。（再通俗一点就是指程序的在某个时刻的运行环境），而我们用户态则是通过切换栈帧来进行上下文的切换。

linux下提供了两个函数来进行上下文的切换，分别是makecontext和swapcontext，makecontext用来创建上下文，swapcontext用来切换上下文。

我们看一个简单的例子
```c++
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
```
首先我们定义了三个上下文，分别是uctx_main，uctx_func1，uctx_func2，其中uctx_main是main函数的上下文，uctx_func1是func1函数的上下文，uctx_func2是func2函数的上下文。当然我们这么说可不算，需要使用makecontext来指定，这里我们指定了uctx_func1的上下文是func1，指定了uctx_func2的上下文是func2，同时指定了uctx_func1的返回时恢复uctx_main的上下文，指定了uctx_func2的返回时恢复uctx_func1的上下文。接下来我们就可以使用swapcontext函数来切换上下文，从而达到不同的执行效果，执行结果如下：
```shell
main: swapcontext(&uctx_main, &uctx_func2)
func2: started
func2: swapcontex(&uctx_func2, &uctx_func1)
func1: started
func1: swapcontex(&uctx_func1, &uctx_func2)
func2:returning
func1:returning
main: exiting
```
我们从结果看可以印证我们的猜想，swapcontext函数可以切换上下文，从而达到不同的执行效果。

有了这些知识作为储备，我们就可以来实现我们的协程了

## 协程（简介）









