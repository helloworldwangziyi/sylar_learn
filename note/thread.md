# 线程类的封装

好的我们来看看线程是怎样进行封装的，直接看看线程类的定义
```c++
class Thread {
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()> cb, const std::string& name);
    ~Thread();

    pid_t getId() const { return m_id;}
    const std::string& getName() const { return m_name;}

    void join();

    static Thread* GetThis();
    static const std::string& GetName();
    static void SetName(const std::string& name);
private:
    Thread(const Thread&) = delete;
    Thread(const Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;

    static void* run(void* arg);
private:
    pid_t m_id = -1;
    pthread_t m_thread = 0;
    std::function<void()> m_cb;
    std::string m_name;

    Semaphore m_semaphore;
};
```
简单来看，我们可以通过承运名看出线程类是如何使用的，我们禁用拷贝构造函数和拷贝赋值运算符，防止意外的拷贝，保证唯一性（简单来说就是线程不会因为拷贝一分为2），我们需要注意的是这个类中的静态成员方法和普通成员方法，至于为什么我们可以在cpp文件中找到答案
```c++
static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOW";
```
首先我们可以看到这样的两个定义，thread_local是c++11中的关键字，表示这个变量是线程局部存储的，也就是说每个线程都有一份，这样我们就可以在每个线程中保存一份线程的信息，static 又将这个变量变成了静态的，这样在整个程序的声明周期之内就都伴随着这个变量，这样我们可以方便的从不同线程中获取线程对象和线程名字。

那么由于这个特性，我们就可以通过静态方法直接获取当前执行的线程实例和线程名字，这也就是我们将这些方法定义为静态的原因。

好的剩下的就是简单的封装了，就不仔细描述了。

我们定义了线程就无法回避线程的同步的问题，为了更好的实现线程同步，我们封装了信号量，互斥锁，读写锁，这些封装的代码实现的并不复杂，就不贴出来了，有兴趣的可以去看看。

哦，对了，这个关于线程模块有一篇文章讲的很好，有兴趣的可以去看看，[sylar线程库](https://www.midlane.top/wiki/pages/viewpage.action?pageId=16416890)

