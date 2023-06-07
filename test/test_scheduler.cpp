#include "sylar.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_fiber() {
    static int s_count = 5;
    SYLAR_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    sleep(1);
    if(--s_count >= 0) {
        sylar::Scheduler::GetThis()->schedule(&test_fiber, sylar::GetThreadId());
    }
}

int main(int argc, char** argv) {
    SYLAR_LOG_INFO(g_logger) << "main";
    sylar::Scheduler sc(1, false, "test");
    sc.start();

    SYLAR_LOG_INFO(g_logger) << "schedule";
    sc.schedule(&test_fiber);
    SYLAR_LOG_INFO(g_logger) << "over";

    sc.stop();
    return 0;
}