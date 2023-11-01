#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "include/user.h"
#include <asm-generic/socket.h>
#include "include/log.h"
#include "include/cJSON.h"


typedef struct ing_req
{
    int port;
    char username[MAX_NAME_SIZE];
    char ing[MAX_NAME_SIZE];
    int quant;
    int status;
}ing_req;



typedef struct supplyer{
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
    ing_req requests[50];
    int req_count;
}supplyer;



void config_supplyer(supplyer* supplyer_, int argc, char* argv[]){
    if(argc != 2){
        logError("port not provided");
    }
    //udp-------------
    if((supplyer_ -> UDP_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        logError("socket");
    }
    int broadcast = 1;
    int opt = 1;
    supplyer_ -> UDP_port = atoi(argv[1]);
    setsockopt(supplyer_ -> UDP_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    setsockopt(supplyer_ -> UDP_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    supplyer_ -> UDP_address.sin_family = AF_INET; 
    supplyer_ -> UDP_address.sin_port = htons(supplyer_ -> UDP_port); 
    supplyer_ -> UDP_address.sin_addr.s_addr = inet_addr("10.0.2.255");
    if((bind(supplyer_ -> UDP_fd, (struct sockaddr *)&supplyer_ -> UDP_address, sizeof(supplyer_ -> UDP_address))) < 0){
        logError("bind");
    }
    //udp--------------
    //tcp-------------
    supplyer_ -> TCP_port = generate_random_port();
    if((supplyer_ -> server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        logError("socket");
    }
    int opt_ = 1;
    setsockopt(supplyer_ -> server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_, sizeof(opt_));
    supplyer_ -> TCP_address.sin_family = AF_INET;
    supplyer_ -> TCP_address.sin_addr.s_addr = INADDR_ANY;
    supplyer_ -> TCP_address.sin_port = htons(supplyer_ -> TCP_port);
    while((bind(supplyer_ -> server_fd, (struct sockaddr *)&supplyer_ -> TCP_address, sizeof(supplyer_ -> TCP_address))) < 0){
        supplyer_ -> TCP_port = generate_random_port();
        supplyer_ -> TCP_address.sin_port = htons(supplyer_ -> TCP_port);
    }
    if(listen(supplyer_ -> server_fd, 20) < 0){
        logError("listen");
    }
    memset(supplyer_ -> user_name, 0, MAX_NAME_SIZE);
    write(STDOUT_FILENO, ">>please enter your username:", 30);
    supplyer_ -> req_count = 0;
}


void send_message(supplyer* supplyer_, char* message){
    sendto(supplyer_ -> UDP_fd, message, strlen(message), 0,(struct sockaddr *)&supplyer_ -> UDP_address, sizeof(supplyer_ -> UDP_address));
}


void username_check(supplyer* supplyer_){
    char username[MAX_NAME_SIZE];
    memset(username, 0, MAX_NAME_SIZE);
    int n = read(STDIN_FILENO, username, MAX_NAME_SIZE);
    username[n-1] = '\0';
    strcpy(supplyer_ -> user_name, username);
    memset(supplyer_ -> buf, 0, BUFFER_SIZE);
    sprintf(supplyer_ -> buf, "%s|%d|%s|", USERNAMECHECK, supplyer_ -> TCP_port, username);
    send_message(supplyer_, supplyer_ -> buf);
    fd_set tmp;
    FD_ZERO(&tmp);
    FD_SET(supplyer_ -> server_fd, &tmp);
    int max_fd = supplyer_ -> server_fd;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    while(1){
        if(select(max_fd + 1, &tmp, NULL, NULL, &tv) < 0){
            logError("select");
        }
        if(FD_ISSET(supplyer_ -> server_fd, &tmp)){
            int new_socket = acceptClient(supplyer_ -> server_fd);
            char tmp_buf[BUF_SIZE];
            memset(tmp_buf, 0, BUF_SIZE);
            recv(new_socket, tmp_buf, BUF_SIZE, 0);
            close(new_socket);
            FD_CLR(new_socket, &tmp);
            char* command = strtok(tmp_buf, DELIM);
            if(strcmp(command, USERNAMEDENIED) == 0){
                write(STDOUT_FILENO, ">>username already exists!please try again>>", 44);
                username_check(supplyer_);
                return;
            }
        }
        else{
            char tmp_message [BUFFER_SIZE];
            memset(tmp_message, 0, BUF_SIZE);
            sprintf(tmp_message, ">>welcome %s as a supplyer\n", supplyer_ -> user_name);
            write(STDOUT_FILENO, tmp_message, BUFFER_SIZE);
            return;
        }
    }
}


void handle_command(supplyer* supplyer_, char* input_line){
    char* command = strtok(input_line, DELIM);
    if(strcmp(command, USERNAMECHECK) == 0){
        char* port_num_str = strtok(NULL, DELIM);
        int port = atoi(port_num_str);
        char* tmp_name = strtok(NULL, DELIM);
        if(port != supplyer_ -> TCP_port){
            if(strcmp(tmp_name, supplyer_ -> user_name) == 0){
                sprintf(supplyer_ -> buf, "%s|", USERNAMEDENIED);
                int fd = connectServer(port);
                send(fd, supplyer_ -> buf, BUFFER_SIZE, 0);
                close(fd);
            }
            
        }
    }
    else if(strcmp(command, SHOW_SUPPLYERS_S) == 0){
        char tmp_msg[BUF_SIZE];
        memset(tmp_msg, 0, BUF_SIZE);
        sprintf(tmp_msg, "%s|%s|%d|", I_AM_SUPPLYAER, supplyer_ -> user_name, supplyer_ -> TCP_port);
        char*port_num_str = strtok(NULL, DELIM);
        int port = atoi(port_num_str);
        int fd = connectServer(port);
        send(fd, tmp_msg, BUFFER_SIZE, 0);
        close(fd);
    }
    else if(strcmp(command, ORDER_EXPIRD) == 0){
        char* port_str = strtok(NULL, DELIM);
        int port = atoi(port_str);
        for(int i = 0 ; i < supplyer_ -> req_count ; i++){
            if(supplyer_->requests[i].port == port){
                supplyer_ -> requests[i].status = 0;
                //order expired
            }

        }
    }
    else if(strcmp(command, REQ_ING_S) == 0){
        if((supplyer_ -> req_count == 0) || (supplyer_ -> requests[supplyer_ -> req_count - 1].status == 0)){
            write(STDOUT_FILENO, "new order!\n", 12);
            char* port_str = strtok(NULL, DELIM);
            int port = atoi(port_str);
            char* ing_name = strtok(NULL, DELIM);
            char* quant_str = strtok(NULL, DELIM);
            int quant = atoi(quant_str);
            char* rest_name = strtok(NULL, DELIM);
            supplyer_ -> requests[supplyer_ -> req_count].status = 1;
            supplyer_ -> requests[supplyer_ -> req_count].port = port;
            supplyer_ -> requests[supplyer_ -> req_count].quant = quant;
            strcpy(supplyer_ -> requests[supplyer_ -> req_count].username, rest_name);
            strcpy(supplyer_ -> requests[supplyer_ -> req_count].ing, ing_name);
            supplyer_ -> req_count += 1;
        }
        else{
            char* port_str = strtok(NULL, DELIM);
            int port = atoi(port_str);
            char resp[BUF_SIZE];
            memset(resp, 0, BUF_SIZE);
            sprintf(resp, "%s|%s|", BUSY, supplyer_ -> user_name);
            int fd = connectServer(port);
            send(fd, resp, BUF_SIZE, 0);
            close(fd);
        }

    }
    else if(strcmp(command, ANSWER_REQ) == 0){
        int req_available = 0;
        int index = -1;
        for(int i = 0 ; i < supplyer_ -> req_count ; i++){
            if(supplyer_ -> requests[i].status == 1){
                index = i;
            }
        }
        if(index == -1){
            write(STDOUT_FILENO, "no requests for ingredients!\n", 30);
        }
        else{
            write(STDOUT_FILENO, ">>your answer (YES/NO): ", 25);
            char tmp_buf[10];
            memset(tmp_buf, 0, 10);
            int x = read(STDIN_FILENO, tmp_buf, 10);
            tmp_buf[x-1] = '\0';
            char msg[BUF_SIZE];
            memset(msg, 0, BUF_SIZE);
            if(strcmp(tmp_buf, "YES") == 0){
                sprintf(msg, "%s|%s|", ING_ACC, supplyer_ -> user_name);
            }
            else{
                sprintf(msg, "%s|%s|", ING_REJ, supplyer_ -> user_name);
            }
            int fd = connectServer(supplyer_ -> requests[index].port);
            send(fd, msg, BUF_SIZE, 0);
        }
    }
}


void run_supplyer(supplyer* supplyer_){
    FD_ZERO(&supplyer_ -> master_set);
    supplyer_ -> max_fd = supplyer_ -> server_fd;
    FD_SET(supplyer_ -> server_fd, &supplyer_ -> master_set);
    FD_SET(supplyer_ -> UDP_fd, &supplyer_ -> master_set);
    FD_SET(STDIN_FILENO, &supplyer_ -> master_set);
    fd_set working_set;
    while (1)
    {
        working_set = supplyer_ -> master_set;
        if(select(supplyer_ -> max_fd + 1, &working_set, NULL, NULL, NULL) < 0){
            logError("select");
        }
        for(int i = 0 ; i <= supplyer_ -> max_fd ; i++){
            if(FD_ISSET(i, &working_set)){
                if(i == STDIN_FILENO){
                    memset(supplyer_ -> buf, 0, BUFFER_SIZE);
                    int x = read(STDIN_FILENO, supplyer_ -> buf, BUFFER_SIZE);
                    supplyer_ -> buf[x - 1] = '\0';
                    concatenate_string(supplyer_ -> buf, "|");
                    handle_command(supplyer_, supplyer_ -> buf);

                }
                else if(i == supplyer_ -> server_fd){
                    int new_socket = acceptClient(supplyer_ -> server_fd);
                    FD_SET(new_socket, &supplyer_ -> master_set);
                    if(new_socket > supplyer_ -> max_fd){
                        supplyer_ -> max_fd = new_socket;
                    }
                }
                else if(i == supplyer_ -> UDP_fd){
                    memset(supplyer_ -> buf, 0, BUFFER_SIZE);
                    recv(supplyer_ -> UDP_fd, supplyer_ -> buf, BUFFER_SIZE, 0);
                    handle_command(supplyer_, supplyer_ -> buf);
                }
                else{
                    memset(supplyer_ -> buf, 0, BUFFER_SIZE);
                    recv(i, supplyer_ -> buf, BUFFER_SIZE, 0);
                    close(i);
                    FD_CLR(i, &supplyer_ -> master_set);
                    handle_command(supplyer_, supplyer_ -> buf);
                }
            }
        }
    }
    
}


int main(int argc, char* argv[]){
    supplyer supplyer_;
    config_supplyer(&supplyer_, argc, argv);
    username_check(&supplyer_);
    run_supplyer(&supplyer_);
}