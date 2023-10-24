#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <time.h>

volatile int stop_thread1 = 0;  
volatile int stop_thread2 = 0;  

void signal_handler(int signum) {
    if (signum == SIGUSR1) {
        stop_thread1 = 1;
    } else if (signum == SIGUSR2) {
        stop_thread2 = 1;
    }
}

char* get_current_time() {
    static char buffer[100];
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    return buffer;
}

void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    printf("[%s] [Thread %lu] ", get_current_time(), pthread_self());
    vprintf(format, args);
    printf("\n");

    va_end(args);
}

void* file_output_thread(void* arg) {
    int fd = open("../file.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd == -1) {
        log_message("Failed to open file");
        return NULL;
    }

    while(!stop_thread1) {
        for (int i = 0; i < 10; ++i) {
            dprintf(fd, "[%s] [Thread %lu] Writing to file: %d\n", get_current_time(), pthread_self(), i);
            sleep(1);
            if (stop_thread1) {
                break;
            }
        }
    }
    close(fd);
    log_message("File output thread terminated.");
    return NULL;
}

void* uart_send_thread(void* arg) {
    int fd = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY); 
    if (fd == -1) {
        log_message("Failed to open UART");
        return NULL;
    }

    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);

    while(!stop_thread2) {
        for (int i = 0; i < 10; ++i) {
            int ret = dprintf(fd, "[%s] [Thread %lu] UART Message\n", get_current_time(), pthread_self());
            if (ret < 0) {
                log_message("Failed to write to UART");
                break;
            }
            sleep(1);
            if (stop_thread2) {
                break;
            }
        }
    }
    close(fd);
    log_message("UART send thread terminated.");
    return NULL;
}

int main(void) {
    pthread_t thread1, thread2;

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    pthread_create(&thread1, NULL, file_output_thread, NULL);
    pthread_create(&thread2, NULL, uart_send_thread, NULL);

    sleep(10);

    pthread_kill(thread1, SIGUSR1);
    pthread_kill(thread2, SIGUSR2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    log_message("End");

    return 0;
}
