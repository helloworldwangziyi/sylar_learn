#include"log.h"
#include<iostream>

int main()
{
    sylar::Logger::ptr logger(new sylar::Logger);
    logger->addAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));

    sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("log_20200519_183409.txt"));
    sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%p%T%m%n"));
    file_appender->setFormatter(fmt);
    file_appender->setLevel(sylar::LogLevel::ERROR);

    logger->addAppender(file_appender);

    for(int i = 0;i<100000; i++)
    {
        SYLAR_LOG_ERROR(logger) << "test macro";
    }
    
    return 0;
}