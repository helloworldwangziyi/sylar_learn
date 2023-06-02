#ifndef __SYLAR_UTIL_H__
#define __SYLAR_UTIL_H__

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>

namespace sylar {

pid_t GetThreadId();
uint32_t GetFiberId();

// Backtrace是一个函数调用栈的工具，可以用来获取当前线程的函数调用栈 size代表最多获取多少个栈帧 skip代表跳过栈帧的个数
void Backtrace(std::vector<std::string>& bt, int size, int skip = 1); 
// BacktraceToString是Backtrace的包装函数，返回一个字符串代表当前线程的函数调用栈 size代表最多获取多少个栈帧 skip代表跳过栈帧的个数 prefix代表每一行输出的前缀
std::string BacktraceToString(int size, int skip = 2, const std::string& prefix = "");

}

#endif