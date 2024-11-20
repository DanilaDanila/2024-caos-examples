#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

char ok_msg[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "ok";

void write_all(int fd, char *msg, size_t len) {
    size_t total = 0;
    while(total < len) {
        total += write(fd, msg + total, len - total);
    }
}

void *process_client(void *args) {
    int client_fd = (int)(args);

    sleep(1);  // типа очень долго что-то делаем
    write_all(client_fd, ok_msg, sizeof(ok_msg));

    // заканчиваем общение с сервером
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);

    return NULL;
}

int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 1) {
        perror("socket");
    }

    // задаем параметры, что будем слушать
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8888),  // хотим BIG ENDIAN
        .sin_addr.s_addr = INADDR_ANY,  // константа, чтоб принимать любые
                                        // входящие соединения
    };

    // сопоставляем себя с параметрами
    bind(socket_fd, (struct sockaddr*)(&addr), sizeof(addr));

    // говорим, что начинаем слушать соединения
    listen(socket_fd, SOMAXCONN);

    for (int i = 1; ; ++i) {
        // подключаем клиента
        int client_fd = accept(socket_fd, NULL, NULL);
        printf("got client %d\n", i);

        pthread_t tid;
        pthread_create(
            &tid,
            NULL,
            process_client,
            (void*)(client_fd)
        );
        pthread_detach(tid);
    }

    // не делайте так (код ниже - unreachable)

    // закрываем сервер
    shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);

    return 0;
}
