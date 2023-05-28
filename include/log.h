#ifndef __SYLAR__LOG_H__
#define __SYLAR__LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <stdarg.h>
#include <map>
#include"singleton.h"
#include"util.h"
#include <time.h>
#include <chrono>
#include <iomanip>

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

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->getRoot()
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->getLogger(name)

namespace sylar 
{

class Logger;
class LoggerManager;


class LogLevel
{
public:
    // 日志级别分为6个等级
    // UNKNOW = 0, DEBUG = 1, INFO = 2, WARN = 3, ERROR = 4, FATAL = 5
    enum Level
    {
        UNKNOW = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    static const char* ToString(LogLevel::Level level); // 将日志级别转换为字符串
    static LogLevel::Level FromString(const std::string& str); // 将字符串转换为日志级别
};

//日志事件
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level
            ,const char* file, int32_t m_line, uint32_t elapse
            , uint32_t thread_id, uint32_t fiber_id, uint64_t time);

    const char* getFile() const { return m_file;}
    int32_t getLine() const { return m_line;}
    uint32_t getElapse() const { return m_elapse;}
    uint32_t getThreadId() const { return m_threadId;}
    uint32_t getFiberId() const { return m_fiberId;}
    uint64_t getTime() const { return m_time;}
    std::string getContent() const { return m_ss.str();}
    std::shared_ptr<Logger> getLogger() const { return m_logger;}
    LogLevel::Level getLevel() const { return m_level;}

    std::stringstream& getSS() { return m_ss;}
    void format(const char* fmt, ...);
    void format(const char* fmt, va_list al);
private:
    const char* m_file = nullptr;  //文件名
    int32_t m_line = 0;            //行号
    uint32_t m_elapse = 0;         //程序启动开始到现在的毫秒数
    uint32_t m_threadId = 0;       //线程id
    uint32_t m_fiberId = 0;        //协程id
    uint64_t m_time = 0;           //时间戳
    std::stringstream m_ss;

    std::shared_ptr<Logger> m_logger;
    LogLevel::Level m_level;
};


class LogEventWrap { // 日志事件包装器
public:
    LogEventWrap(LogEvent::ptr e);
    ~LogEventWrap();
    LogEvent::ptr getEvent() const { return m_event;}
    std::stringstream& getSS();
private:
    LogEvent::ptr m_event;
};


//日志格式器
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    LogFormatter(const std::string& pattern);

    /**
     * @brief 构造函数
     * @param[in] pattern 格式模板
     * @details 
     *  %m 消息
     *  %p 日志级别
     *  %r 累计毫秒数
     *  %c 日志名称
     *  %t 线程id
     *  %n 换行
     *  %d 时间
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %F 协程id
     *  %N 线程名称
     *
     *  默认格式 "%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"
     *  年月日 时分秒 线程id 线程名称 协程id 日志级别 日志名称 文件名:行号 消息 换行
     */

    //%t    %thread_id %m%n
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };

    void init();

    bool isError() const { return m_error;}
    const std::string getPattern() const { return m_pattern;}
private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
    bool m_error = false;
};

//日志输出地
class LogAppender {
public:
    typedef std::shared_ptr<LogAppender> ptr;
    virtual ~LogAppender() {}

    /**
     * @brief 写入日志
     * @param[in] logger 日志器
     * @param[in] level 日志级别
     * @param[in] event 日志事件
     */
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    virtual std::string toYamlString() = 0;

    void setFormatter(LogFormatter::ptr val) { m_formatter = val;}
    LogFormatter::ptr getFormatter() const { return m_formatter;}

    LogLevel::Level getLevel() const { return m_level;}
    void setLevel(LogLevel::Level val) { m_level = val;}
protected:
    LogLevel::Level m_level = LogLevel::DEBUG;
    LogFormatter::ptr m_formatter;
};


//日志器
class Logger : public std::enable_shared_from_this<Logger> {
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> ptr;

    Logger(const std::string& name = "root");
    void log(LogLevel::Level level, LogEvent::ptr event);

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);

    // 这段等实现了Appender再说
    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppenders();
    LogLevel::Level getLevel() const { return m_level;}
    void setLevel(LogLevel::Level val) { m_level = val;}

    const std::string& getName() const { return m_name;}

    void setFormatter(LogFormatter::ptr val);
    void setFormatter(const std::string& val);
    LogFormatter::ptr getFormatter();

    std::string toYamlString();
private:
    std::string m_name;                     //日志名称
    LogLevel::Level m_level;                //日志级别
    std::list<LogAppender::ptr> m_appenders;//Appender集合
    LogFormatter::ptr m_formatter;
    Logger::ptr m_root;
};

//输出到控制台的Appender
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
};

//定义输出到文件的Appender
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;

    // 文件输出器名，最大文件大小设置为1MB
    /*
    * @brief 构造函数
    * @param[in] filename 文件名
    * @param[in] maxFileSize 文件最大大小
    */
    FileLogAppender(const std::string& filename, uint64_t maxFileSize = 1*1024*1024);
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;

    //重新打开文件，文件打开成功返回true
    bool reopen();
private:
    std::string m_filename;
    std::ofstream m_filestream;
    uint64_t m_maxFileSize = 0;     //文件最大大小
};


class LoggerManager {
public:
    LoggerManager();
    Logger::ptr getLogger(const std::string& name);

    void init();
    Logger::ptr getRoot() const { return m_root;}

    std::string toYamlString();
private:
    std::map<std::string, Logger::ptr> m_loggers;
    Logger::ptr m_root;
};

typedef sylar::Singleton<LoggerManager> LoggerMgr;

}





#endif