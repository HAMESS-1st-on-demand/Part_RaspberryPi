#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

void* thread_function1(void* arg) {
    printf("Thread 1 시작\n");
    sleep(10);
    printf("Thread 1 종료\n");
    return NULL;
}

void* thread_function2(void* arg) {
    printf("Thread 2 시작\n");
    sleep(5);
    printf("Thread 2 종료\n");
    return NULL;
}

int main(void) {
    pthread_t thread1, thread2;

    // 스레드 생성
    pthread_create(&thread1, NULL, thread_function1, NULL);
    pthread_create(&thread2, NULL, thread_function2, NULL);

    // 잠시 기다린 후 스레드가 살아있는지 확인
    sleep(3);
    
    if(!pthread_kill(thread1, 0)) {
        printf("Thread 1이 아직 실행 중입니다. 종료합니다.\n");
        pthread_kill(thread1, SIGTERM);
    }

    if(!pthread_kill(thread2, 0)) {
        printf("Thread 2가 아직 실행 중입니다. 종료합니다.\n");
        pthread_kill(thread2, SIGTERM);
    }

    // 스레드가 종료될 때까지 기다림
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}
