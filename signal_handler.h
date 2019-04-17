#pragma once
//善后工作
void do_clear_work();
//错误处理函数
void process_signal(int signo);
//捕捉一些错误信号并调用处理函数处理
int set_signal_handler();