// gcc ss.c -o ss -lpthread
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#define MAX_CLIENTS 65500
#define CHUNK_SIZE 1000
#define MAX_LENGTH 20
#define PORT 8080

// Глобальные переменные
char charset[64]; // Набор символов для перебора
int charset_size; // Размер набора символов
char current[MAX_LENGTH + 1]; // Текущая комбинация
char loose[MAX_LENGTH + 1]; // Текущая комбинация
pthread_mutex_t lock; // Мьютекс для синхронизации
int client_sockets[MAX_CLIENTS]; // Сокеты клиентов
int client_count = 0; // Количество подключенных клиентов
bool found = false; // Флаг, указывающий, найдена ли комбинация

time_t begin,end;
double time_spent;

void handle_sigtstp(int signum) {
    printf("[!] Current words - %s : signal %d\n",current,signum);
    printf("Time 1 client %lf. Clients=%d. Calc %d pass. Result=%.01lf pass/sec \n",time_spent,client_count,CHUNK_SIZE,CHUNK_SIZE*client_count/time_spent);
}

// Функция для инициализации набора символов
void init_charset() {
    int index = 0;
    for (char c = 'a'; c <= 'z'; c++) charset[index++] = c;
//    for (char c = 'A'; c <= 'Z'; c++) charset[index++] = c;
    for (char c = '1'; c <= '9'; c++) charset[index++] = c;
    charset[index++] = '0';
    
    charset_size = index;
    for(int i=0;i<MAX_LENGTH+1;i++)
	loose[i]='\0';
}

// Функция для генерации следующей комбинации
bool next_combination(char *str) {
    int length = strlen(str);

    // Если строка пустая, начинаем с первого символа
    if (length == 0) {
        str[0] = charset[0];
        str[1] = '\0';
        return true;
    }

    // Перебор символов с конца строки
    for (int i = length - 1; i >= 0; i--) {
        char *pos = strchr(charset, str[i]);
        if (pos != NULL && pos[1] != '\0') {
            str[i] = pos[1]; // Увеличиваем текущий символ
            return true;
        } else {
            str[i] = charset[0]; // Сбрасываем текущий символ
        }
    }

    // Если все символы были последними в наборе, увеличиваем длину строки
    if (length < MAX_LENGTH) {
        str[length] = charset[0]; // Добавляем новый символ
        str[length + 1] = '\0';
        return true;
    }

    // Если достигнута максимальная длина и все комбинации исчерпаны
    return false;
}

// Функция для обработки клиента
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    char buffer[MAX_LENGTH + 1];

    while (!found) {
        pthread_mutex_lock(&lock);
	if( strlen(loose) != 0 )
	{
	  strncpy(buffer, loose, MAX_LENGTH);
	  buffer[MAX_LENGTH] = '\0';
	  for(int i=0;i<MAX_LENGTH+1;i++)
		loose[i]='\0';
	  printf("Send loose - %s\n",buffer);
	}else{
    	    // Отправка текущей комбинации клиенту
	    strncpy(buffer, current, MAX_LENGTH);
		buffer[MAX_LENGTH] = '\0';

    	    // Генерация следующей комбинации для следующего клиента
    	    for (int i = 0; i < CHUNK_SIZE; i++) {
        	    if (!next_combination(current)) {
            	    found = true; // Все комбинации исчерпаны
            	    break;
    	        }
    	    }
	}
        pthread_mutex_unlock(&lock);
        //printf("send - %s\n",buffer);
        
        begin =  time(NULL);//clock();
        // Отправка текущей комбинации клиенту
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            perror("Ошибка отправки данных");
            break;
        }

        // Получение ответа от клиента
        char response[256];
        int bytes_received = recv(client_socket, response, sizeof(response), 0);
        if (bytes_received < 0) {
            perror("Ошибка получения данных");
            client_count--;
            break;
        }else if (bytes_received == 0) {
                printf("Клиент отключился: %d buffer from: %s\n",client_socket,buffer);
                strcpy(loose,buffer);
                client_count--;
        //        printf("Save loos - %s\n",buffer);
                break;
        }

        if (strcmp(response, "FOUND") == 0) {
            printf("Комбинация найдена клиентом %d\n", client_socket);
            found = true;
            break;
        } else if (strcmp(response, "DONE") == 0) {
            // Клиент завершил перебор, продолжаем
        }
        else{
    	    printf("%s\n",response);//print password found
    	}
    	end =  time(NULL);//clock();
    	time_spent = (double)(end - begin);// / CLOCKS_PER_SEC;
    }

    close(client_socket);
    pthread_exit(NULL);
}

int main() {
    // Инициализация набора символов
    init_charset();
    
    struct sigaction sa;
    sa.sa_handler = handle_sigtstp;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Ошибка при установке обработчика сигнала");
        return 1;
    }

    // Ввод начальной позиции
    //printf("Введите начальную комбинацию (максимум %d символов): ", MAX_LENGTH);
    //scanf("%s", current);
    current[0]=charset[0];
    current[1]='\0';

    // Инициализация мьютекса
    pthread_mutex_init(&lock, NULL);

    // Создание сокета
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Ошибка создания сокета");
        exit(1);
    }

    // Настройка адреса сервера
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Привязка сокета
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка привязки сокета");
        exit(1);
    }

    // Прослушивание подключений
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Ошибка прослушивания");
        exit(1);
    }

    printf("Сервер запущен на порту %d\n", PORT);

    // Принятие подключений от клиентов
    while (!found) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Ошибка принятия подключения");
            continue;
        }

	printf("Клиент подключен: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        pthread_mutex_lock(&lock);
        client_sockets[client_count++] = client_socket;
        pthread_mutex_unlock(&lock);

        // Создание потока для обработки клиента
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, &client_sockets[client_count - 1]);
    }

    // Закрытие сокетов клиентов
    for (int i = 0; i < client_count; i++) {
	printf("socket close\n");
        close(client_sockets[i]);
    }

    // Закрытие сокета сервера
    close(server_socket);
    pthread_mutex_destroy(&lock);

    printf("Сервер завершил работу\n");
    return 0;
}
