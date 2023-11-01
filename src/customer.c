#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "include/cJSON.h"
#include <unistd.h>
#include "include/user.h"
#include "include/log.h"
#include <termios.h>
#include <unistd.h>





typedef struct
{
    int UDP_port;
    int TCP_port;
    int UDP_fd;
    int server_fd;
    fd_set master_set;
    struct sockaddr_in UDP_address;
    struct sockaddr_in TCP_address;
    int max_fd;
    char buf[BUFFER_SIZE];
    char user_name[MAX_NAME_SIZE];
    char menu[BUF_SIZE];
    
}customer;







void enableRawMode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void fillMenuFromJson(customer* cust)
{
    FILE* file = fopen("./src/recipes.json", "r");
    if (!file) {
        printf("Failed to open JSON file.\n");
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        printf("Failed to allocate memory.\n");
        fclose(file);
        return;
    }

    size_t bytesRead = fread(buffer, 1, fileSize, file);
    fclose(file);

    if (bytesRead != fileSize) {
        printf("Failed to read JSON file.\n");
        free(buffer);
        return;
    }

    buffer[fileSize] = '\0';

    cJSON* root = cJSON_Parse(buffer);
    if (!root) {
        printf("Failed to parse JSON.\n");
        free(buffer);
        return;
    }

    cJSON* foodName;
    int itemNumber = 1;
    cJSON_ArrayForEach(foodName, root) {
        if (foodName->string) {
            char item[BUF_SIZE];
            snprintf(item, sizeof(item), "%d.%s", itemNumber, foodName->string);
            strncat(cust->menu, item, sizeof(cust->menu) - strlen(cust->menu) - 1);
            strncat(cust->menu, "\n", sizeof(cust->menu) - strlen(cust->menu) - 1);
            itemNumber++;
        }
    }

    cJSON_Delete(root);
    free(buffer);
}









void config_customer(customer* customer_, int argc, char* argv[]){
    if(argc != 2){
        logError("port not provided");
    }
    //udp-------------
    if((customer_ -> UDP_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        logError("socket");
    }
    int broadcast = 1;
    int opt = 1;
    customer_ -> UDP_port = atoi(argv[1]);
    setsockopt(customer_ -> UDP_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    setsockopt(customer_ -> UDP_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    customer_ -> UDP_address.sin_family = AF_INET; 
    customer_ -> UDP_address.sin_port = htons(customer_ -> UDP_port); 
    customer_ -> UDP_address.sin_addr.s_addr = inet_addr("10.0.2.255");
    if((bind(customer_ -> UDP_fd, (struct sockaddr *)&customer_ -> UDP_address, sizeof(customer_ -> UDP_address))) < 0){
        logError("bind");
    }
    //udp--------------
    //tcp-------------
    customer_ -> TCP_port = generate_random_port();
    if((customer_ -> server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        logError("socket");
    }
    int opt_ = 1;
    setsockopt(customer_ -> server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_, sizeof(opt_));
    customer_ -> TCP_address.sin_family = AF_INET;
    customer_ -> TCP_address.sin_addr.s_addr = INADDR_ANY;
    customer_ -> TCP_address.sin_port = htons(customer_ -> TCP_port);
    while((bind(customer_ -> server_fd, (struct sockaddr *)&customer_ -> TCP_address, sizeof(customer_ -> TCP_address))) < 0){
        customer_ -> TCP_port = generate_random_port();
        customer_ -> TCP_address.sin_port = htons(customer_ -> TCP_port);
    }
    if(listen(customer_ -> server_fd, 20) < 0){
        logError("listen");
    }
    memset(customer_ -> user_name, 0, MAX_NAME_SIZE);
    memset(customer_ -> menu, 0, BUF_SIZE);
    fillMenuFromJson(customer_);
    write(STDOUT_FILENO, ">>please enter your username:", 30);
}


void send_message(customer* customer_, char* message){
    sendto(customer_ -> UDP_fd, message, strlen(message), 0,(struct sockaddr *)&customer_ -> UDP_address, sizeof(customer_ -> UDP_address));
}


void username_check(customer* customer_){
    char username[MAX_NAME_SIZE];
    memset(username, 0, MAX_NAME_SIZE);
    int n = read(STDIN_FILENO, username, MAX_NAME_SIZE);
    username[n-1] = '\0';
    strcpy(customer_ -> user_name, username);
    memset(customer_ -> buf, 0, BUFFER_SIZE);
    sprintf(customer_ -> buf, "%s|%d|%s|", USERNAMECHECK, customer_ -> TCP_port, username);
    send_message(customer_, customer_ -> buf);
    fd_set tmp;
    FD_ZERO(&tmp);
    FD_SET(customer_ -> server_fd, &tmp);
    int max_fd = customer_ -> server_fd;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    while(1){
        if(select(max_fd + 1, &tmp, NULL, NULL, &tv) < 0){
            logError("select");
        }
        if(FD_ISSET(customer_ -> server_fd, &tmp)){
            int new_socket = acceptClient(customer_ -> server_fd);
            char tmp_buf[BUF_SIZE];
            memset(tmp_buf, 0, BUF_SIZE);
            recv(new_socket, tmp_buf, BUF_SIZE, 0);
            close(new_socket);
            FD_CLR(new_socket, &tmp);
            char* command = strtok(tmp_buf, DELIM);
            if(strcmp(command, USERNAMEDENIED) == 0){
                write(STDOUT_FILENO, ">>username already exists!please try again>>", 44);
                username_check(customer_);
                return;
            }
        }
        else{
            char tmp_message [BUFFER_SIZE];
            memset(tmp_message, 0, BUF_SIZE);
            sprintf(tmp_message, ">>welcome %s as a customer\n", customer_ -> user_name);
            write(STDOUT_FILENO, tmp_message, BUFFER_SIZE);
            return;
        }
    }
}



void handle_command(customer* customer_, char* input_line){
    char tmp_buf[BUF_SIZE];
    memset(tmp_buf, 0, BUF_SIZE);
    char* command = strtok(input_line, DELIM);
    if(strcmp(command, USERNAMECHECK) == 0){
        char* port_num_str = strtok(NULL, DELIM);
        int port = atoi(port_num_str);
        char* tmp_name = strtok(NULL, DELIM);
        if(port != customer_ -> TCP_port){
            if(strcmp(tmp_name, customer_ -> user_name) == 0){
                sprintf(customer_ -> buf, "%s|", USERNAMEDENIED);
                int fd = connectServer(port);
                send(fd, customer_ -> buf, BUFFER_SIZE, 0);
                close(fd);
            }
            
        }
    }
    else if(strcmp(command, SHOW_MENU) == 0){
        write(STDOUT_FILENO, customer_ -> menu, BUF_SIZE);
    }
    else if(strcmp(command, SHOW_RESTAURANTS) == 0){
        char msg [BUF_SIZE];
        memset(msg, 0, BUF_SIZE);
        sprintf(msg, "%s|%d|", SHOW_RESTAURANTS_, customer_ -> TCP_port);
        send_message(customer_, msg);
    
    }
    else if(strcmp(command, I_AM_RESTAURANT) == 0){
        char* restaurant_name = strtok(NULL, DELIM);
        char* port_str = strtok(NULL, DELIM);
        char msg [BUF_SIZE];
        memset(msg, 0, BUF_SIZE);
        sprintf(msg, "%s %s\n", restaurant_name, port_str);
        write(STDOUT_FILENO, msg, BUF_SIZE);
    }
    else if(strcmp(command, ORDER_FOOD_C) == 0){
        write(STDOUT_FILENO, ">>what food do you want?", 25);
        char food_name[MAX_NAME_SIZE];
        memset(food_name, 0, MAX_NAME_SIZE);
        int n = read(STDIN_FILENO, food_name, MAX_NAME_SIZE);
        food_name[n-1] = '\0';
        write(STDOUT_FILENO, ">>port of restaurant : ", 24);
        char port_str[6];
        memset(port_str, 0, 6);
        int x = read(STDIN_FILENO, port_str, 6);
        port_str[x-1] = '\0';
        int port = atoi(port_str);
        char tmp_msg[BUF_SIZE];
        memset(tmp_msg, 0, BUF_SIZE);
        sprintf(tmp_msg, "%s|%d|%s|%s|", ORDER_FOOD_R, customer_ -> TCP_port, food_name, customer_ -> user_name);
        int fd = connectServer(port);
        send(fd, tmp_msg, BUF_SIZE, 0);
        close(fd);
        fd_set tmp_fd_set;
        FD_ZERO(&tmp_fd_set);
        FD_SET(customer_ -> server_fd, &tmp_fd_set);
        int max_fd = customer_ -> server_fd;
        struct timeval tv;
        tv.tv_sec = 120;
        tv.tv_usec = 0;
        write(STDOUT_FILENO, "waiting for restaurant's response...\n", 37);
        enableRawMode();
        while(1){
            if(select(max_fd + 1, &tmp_fd_set, 0, 0, &tv) < 0){
                logError("select");
            }
            if(FD_ISSET(customer_ -> server_fd, &tmp_fd_set)){
                int new_sock = acceptClient(customer_ -> server_fd);
                char recv_msg[BUF_SIZE];
                memset(recv_msg, 0, BUF_SIZE);
                recv(new_sock, recv_msg, BUF_SIZE, 0);
                char disp_msg[BUF_SIZE];
                memset(disp_msg, 0, BUF_SIZE);
                char* ans = strtok(recv_msg, DELIM);
                char* rest_name = strtok(NULL, DELIM);
                if(strcmp(ans, FOOD_ACCEPTED) == 0){
                    sprintf(disp_msg, "%s restaurant accepted your order and your food is ready\n", rest_name);
                }
                else{
                    sprintf(disp_msg, "%s restaurant rejected your order\n", rest_name);
                }
                write(STDOUT_FILENO, disp_msg, BUF_SIZE);
                break;
            }
            else{
                logInfo("timeOut");
                char msg[BUF_SIZE];
                memset(msg, 0, BUF_SIZE);
                sprintf(msg, "%s|%d|", ORDER_EXPIRD, customer_ -> TCP_port);
                int fd_ = connectServer(port);
                send(fd_, msg, BUF_SIZE, 0);
                close(fd);
                break;
            }
        }
        disableRawMode();
    }
    else if(strcmp(command, REST_CLOSED) == 0){
        char* name = strtok(NULL, DELIM);
        char msg[BUF_SIZE];
        memset(msg, 0, BUF_SIZE);
        sprintf(msg, "%s restaurant closed\n", name);
        write(STDOUT_FILENO, msg, BUF_SIZE);
    }
}






void run_customer(customer* customer_){
    FD_ZERO(&customer_ -> master_set);
    customer_ -> max_fd = customer_ -> server_fd;
    FD_SET(customer_ -> server_fd, &customer_ -> master_set);
    FD_SET(customer_ -> UDP_fd, &customer_ -> master_set);
    FD_SET(STDIN_FILENO, &customer_ -> master_set);
    fd_set working_set;
    while (1)
    {
        working_set = customer_ -> master_set;
        if(select(customer_ -> max_fd + 1, &working_set, NULL, NULL, NULL) < 0){
            logError("select");
        }
        for(int i = 0 ; i <= customer_ -> max_fd ; i++){
            if(FD_ISSET(i, &working_set)){
                if(i == STDIN_FILENO){
                    memset(customer_ -> buf, 0, BUFFER_SIZE);
                    int x = read(STDIN_FILENO, customer_ -> buf, BUFFER_SIZE);
                    customer_ -> buf[x - 1] = '\0';
                    concatenate_string(customer_ -> buf, "|");
                    handle_command(customer_, customer_ -> buf);

                }
                else if(i == customer_ -> server_fd){
                    int new_socket = acceptClient(customer_ -> server_fd);
                    FD_SET(new_socket, &customer_ -> master_set);
                    if(new_socket > customer_ -> max_fd){
                        customer_ -> max_fd = new_socket;
                    }
                }
                else if(i == customer_ -> UDP_fd){
                    memset(customer_ -> buf, 0, BUFFER_SIZE);
                    recv(customer_ -> UDP_fd, customer_ -> buf, BUFFER_SIZE, 0);
                    handle_command(customer_, customer_ -> buf);
                }
                else{
                    char msg[BUFFER_SIZE];
                    memset(msg, 0, BUF_SIZE);
                    // memset(customer_ -> buf, 0, BUFFER_SIZE);
                    recv(i, msg, BUFFER_SIZE, 0);
                    close(i);
                    FD_CLR(i, &customer_ -> master_set);
                    handle_command(customer_, msg);
                }
            }
        }
    }
}

int main(int argc, char* argv[]){
    customer customer_;
    config_customer(&customer_, argc, argv);
    username_check(&customer_);
    run_customer(&customer_);
}