# log 日志模块
## LogLevel 日志级别

日志级别分为UNKNOW DEBUG INFO WARN ERROR FATAL 这6个级别使用

一个enum枚举类型定义。该类有两个静态方法ToString和FromString 

作用就是将字符转为枚举类型和将枚举类型转为字符,使用方法也非常简

单 如下：

```C++
#include"log.h"
#include<iostream>

int main()
{
    sylar::LogLevel::Level level = sylar::LogLevel::FromString("DEBUG");
    std::cout << sylar::LogLevel::ToString(level) << std::endl;
}
```
我们可以使用FromString方法将字符转为枚举，这样我们就定义当前日志级别为DEBUG，并且可以使用ToString将当前定义的日志级别打印出来，实现方法非常简单不再赘言，log.cpp文件中有详细说明。

## LogEvent 日志事件
我们看类的定义就可以知道这个类定义了日志事件，比如获取文件，行号，线程号，日志器等等，同时定义了两个format方法，这两方法是为了格式化日志消息，我们看一下实现。
```c++
void LogEvent::format(const char* fmt, ...) {
    va_list al;
    va_start(al, fmt);
    format(fmt, al);
    va_end(al);
}

void LogEvent::format(const char* fmt, va_list al) {
    char* buf = nullptr;
    int len = vasprintf(&buf, fmt, al);
    if(len != -1) {
        m_ss << std::string(buf, len);
        free(buf);
    }
}
```
这里涉及一个小知识点就是可变参数下面看两个小例子会更好的理解 上面的设计实现
```c++
#include<iostream>
#include<stdarg.h>
using namespace std;

string format(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsprintf(buf, fmt, args);
    va_end(args);
    return buf;
}

int sum(int s, ...)
{
    va_list args;
    va_start(args, s);
    int sum1 = s;
    int i = 0;
    while (i < 10)
    {
        int n = va_arg(args, int);
        sum1 += n;
        i++;
    }
    va_end(args);
    return sum1;
}


int main()
{
    format("hello %s", "world");
    auto val = sum(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    cout << val << endl;

}
```
上面两个案例的输出一个是hello world 一个是 55 那么可以看出来可变参数的使用方法就是，
1. 列出可变参数列表（va_list args）
2. 初始化可变参数列表（va_start(args, fmt)）
3. 使用可变参数 比如 格式化输出到buf 或者 求和（获取可变参数）va_arg(args, int) 这表示获取一个int类型的可变参数
4. 清理参数列表 va_end(args); 

那么我们回到LogEvent类的实现，我们可以看到format方法的实现就是使用了vasprintf方法，这个方法的作用就是将可变参数格式化输出到buf中，然后我们将buf中的

内容放到m_ss中，这样就实现了日志消息的格式化输出，而这个格式可以由我们自己指定。

测试：
```c++
    sylar::LogEvent event(nullptr, sylar::LogLevel::DEBUG, __FILE__, __LINE__, 0, 0, 0, time(0));
    event.getSS() << "hello sylar log"; // 将日志内容写入到日志事件中
    event.format("%s", "hello sylar log"); // 格式化写入日志内容
    
    std::cout << event.getContent() << std::endl; // 获取日志内容
```
这样得到的结果就是hello sylar loghello sylar log 为啥是两个粘在一起？因为我们第一次getSS时写入没加换行符，format的结果就拼到一起了，以至于我们获取日志内容的时候得到这样的结果.我们也看到日志事件这个类是包含日志级别的，我们可以获取但是无法直接修改，因为const不允许这么做，而且返回的是值不是引用所以无法直接修改。

上面的案例其实并不是一个好的实现，因为在类中我们定义了相匹配的智能指针，使用智能指针进行这些操作是更好的选择。

```c++
    // 初始化一个日志事件
    std::shared_ptr<sylar::LogEvent> ptr = std::make_shared<sylar::LogEvent>(nullptr, sylar::LogLevel::DEBUG, __FILE__, __LINE__, 0, 0, 0, time(0));
    ptr->format("test %s", "hello world");
    std::cout << ptr->getContent() << std::endl;
```

## LogFormatter 日志格式化器
这个类的作用就是将日志事件格式化输出，我们看一下这个类的定义里比较有意思的部分
```c++
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };

```
类里面又嵌套一个类，在给定的代码中，FormatItem被定义为LogFormatter类的内部类。它被用作LogFormatter类的私有成员变量m_items的元素类型。通过将FormatItem定义为内部类，它可以直接访问LogFormatter类的私有成员变量和方法，并与LogFormatter类的其他部分进行紧密的协作。这样可以更好地组织和管理与日志格式化相关的功能和逻辑。

这个类一会要重点说明的是format方法 他会将日志格式化输出，通过传递logger（这个一会再说） 输出流对象 os， 日志级别，要输出的日志事件，均由我们自己指定。

void init();不要小看这个方法，这个方法用于解析日志格式，是整个类最重要的核心功能。这里不做详细解释，因为代码太长，但是核心思想大致如下：
1. 遍历日志格式字符串，当遇到%时，就将%后面的字符解析出来，然后将解析出来的字符转为对应的FormatItem对象，然后将这个对象放到m_items中，这样就实现了日志格式的解析。
2. 当遇到非%字符时，就将这个字符转为普通的字符串，然后将这个字符串转为FormatItem对象，然后将这个对象放到m_items中，这样就实现了日志格式的解析。
3. 通过上面的解析，我们就可以将日志格式字符串解析为一个个的FormatItem对象，然后我们就可以通过这些对象将日志格式化输出了。
4. 我们在解析的时候检查日志格式是否合法，如果不合法就将is_error设置为true，这样我们就可以在外部检查日志格式是否合法，如果不合法就不要使用这个日志格式了。 

我们现在回来说说format方法，这个方法需要在子类中重写，我们看看他有那些子类,子类太多且结构相同，我们看一个就行。
```c++
class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string& str = "") {}
    void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override {
        os << event->getContent();
    }
};
```
MessageFormatItem 这个类继承LogFormatter::FormatItem 。重写format方法，将对应日志事件的内容输出到流对象中，这样就实现了日志的格式化输出。
其中os 是ostream 我们后面可以将stringstream 和 ofstream子类传递进来，通过多态机制，我们无需对代码进行修改，就可以将日志内容输出到不同的地方，这就是多态的好处。这里ostream 是 ofstream 和 stringstream 的父类。我们做的就是把内容写入流，接下来就不用管了。

然后是这个外层的format

```c++
std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    std::stringstream ss;
    for(auto& i : m_items) {
        i->format(ss, logger, level, event);
    }
    return ss.str();
}
```
简单介绍调用的思路，我们在类初始化的时候解析传入的字符串模式到m_items中，之后将m_items中解析的模式结合日志事件，日志级别输出到stringstream ss中最终返回，m_items中装的就是我们上面写的MessageFormatItem之类的模式对象.

## LogAppender

简单来说就是将日志输出到不同的地方，比如控制台，文件
这个类的主要实现方法就是不同的输出地，都要单独继承这个类来重写log方法，我们以FileLogAppender 的log为例子来说明
```c++
void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        m_filestream << m_formatter->format(logger, level, event);
    }
}
```
这个log函数接收日志器，日志级别， 日志事件为参数，当日志级别大于输出器的日志级别时，就输出到文件流中，这里的m_formatter是我们在初始化的时候传入的，这个是日志格式化器，我们将日志格式化输出到文件流中，这样就实现了日志的输出到文件中。

我们从这个函数中可以看到，日志的输出器是依赖日志的格式化器的，我们将格式化之后的内容输出到文件流就是最好的体现。

## Logger 日志器

当我们大致了解上面的LogAppender和LoggerFormater这两个类之后就可以，开始看看logger是怎么实现的了。

首先他的定义就非常与众不同
```c++
class Logger : public std::enable_shared_from_this<Logger>
```
这个类继承了std::enable_shared_from_this 这个类的作用就是可以通过this指针获取当前对象的智能指针,我们可以通过shared_from_this()获取当前对象的智能指针，这样就可以在类内部使用智能指针了，这样就可以避免内存泄漏和重复定义智能指针的问题。

这个类的核心方法是log方法，规定我们如何输出日志，看看具体实现
```c++
void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
    if(level >= m_level) {
        auto self = shared_from_this(); // shared_from_this()返回一个指向当前对象的shared_ptr
        if(!m_appenders.empty()) { // 如果有日志输出器
            for(auto& i : m_appenders) { // 遍历日志输出器
                i->log(self, level, event);
            }
        } else if(m_root) { // 如果没有日志输出器，但是有根日志器
            m_root->log(level, event); // 就调用根日志器的log方法
        }
    }
}
```
首先判断日志级别是否达到输出条件，auto self 就是当前logger的智能指针，当m_appenders不空就调用LogAppender的log方法，将日志输出到对应的地方，如果m_appenders为空，但是有根日志器，就调用根日志器的log方法，这样就实现了日志的输出。

Logger log -> LogAppender log -> LogFormatter format -> FormatItem format
这个流程就可以比较清晰的描述日志的输出过程了。

哦对了，Logger的日志级别在初始化的时候默认为DEBUG，我们可以通过setLevel方法修改日志级别，这样就可以控制日志的输出了。

## LogEventWrap 日志事件包装器

我们理解这个类就简单的理解为，日志事件的包装就行， 看看这个类的定义
```c++
class LogEventWrap { // 日志事件包装器
public:
    LogEventWrap(LogEvent::ptr e);
    ~LogEventWrap();
    LogEvent::ptr getEvent() const { return m_event;}
    std::stringstream& getSS();
private:
    LogEvent::ptr m_event;
};
```
这个类的作用就是将日志事件包装起来，这样我们就可以在类外部使用这个类，然后在类外部使用这个类的getSS方法，将日志内容写入到日志事件中，这样就实现了日志的输出。

但是最有意思的是这个类的析构函数
```c++
LogEventWrap::~LogEventWrap() {
    // 析构的时候自动输出日志
    m_event->getLogger()->log(m_event->getLevel(), m_event);
}
```
瞧瞧都做了什么？我们在析构的时候自动调用日志器的log方法，这样就可以将日志自动输出了。这也为我们后边使用宏定义来实现日志的自动输出提供了思路。

## 关于头文件上方的宏定义

```c++
#define SYLAR_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
                        __FILE__, __LINE__, 0, sylar::GetThreadId(),\
                sylar::GetFiberId(), time(0)))).getSS()

#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, \
                        __FILE__, __LINE__, 0, sylar::GetThreadId(),\
                sylar::GetFiberId(), time(0)))).getEvent()->format(fmt, __VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...)  SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, fmt, __VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, fmt, __VA_ARGS__)

```
为什么要使用LogEventWrap呢？因为他的析构函数中实现了日志的自动输出，这样我们就可以在类外部使用这个宏定义，然后在类外部使用这个类的getSS方法，将日志内容写入到日志事件中，这样就实现了日志的输出。

## 测试

有了之前的LogEventWrap类，我们就可以在这里实现日志的自动输出了， 下面对之前的所有功能，集中进行测试，测试文件为test.cpp 生成可执行文件名为test_log

```c++
    auto l = sylar::LoggerMgr::GetInstance()->getLogger("system");
    l->setFormatter("%d%T%m%T%n");
    sylar::StdoutLogAppender::ptr stdout_appender(new sylar::StdoutLogAppender);
    sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%m%T%n"));
    stdout_appender->setFormatter(fmt);
    l->addAppender(sylar::LogAppender::ptr(stdout_appender));

    SYLAR_LOG_INFO(l) << "xxx";
```
这里写一个我自己调试遇到的小坑，如上面的代码所示，我们通过LoggerMgr创建一个用于管理日志器的单例，然后通过getLogger方法获取一个日志器，这里我获取的是system日志器，然后设置日志格式，这里我设置的是%d%T%m%T%n，这个格式是我自己定义的，然后创建一个StdoutLogAppender，这个是输出到控制台的输出器，然后创建一个日志格式化器，这个格式化器的格式和上面的一样，然后将这个格式化器设置到StdoutLogAppender中，然后将StdoutLogAppender添加到system日志器中，这样就实现了日志的输出到控制台。 虽然这里的测试代码可以按照我们的设想输出日志，但是如果只保留这三行代码是无法正常运行的
```c++
    auto l = sylar::LoggerMgr::GetInstance()->getLogger("system");
    l->setFormatter("%d%T%m%T%n");
    SYLAR_LOG_INFO(l) << "xxx";
```
看似我们设置了日志格式，然后输出日志，但是实际上无法正常输出我们设定的格式，为啥？参考一下上面的正确答案就可以知道，我们得到的这个logger是没有输出器的，没有输出器的logger会使用根日志器的输出格式，这就使得我们定义的输出格式失效，所以我们需要将输出器添加到logger中，这样才能正常输出日志。

日志模块的主要功能就结束了，当然你还会看到一些零星的toyamlstring方法，请暂时忽视，这是下一模块的内容。