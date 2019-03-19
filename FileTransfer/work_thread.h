#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

/*
 *启动线程
 */ 
void thread_start( int c);

/*
 *工作线程
 */
void* work_thread(void * arg);
