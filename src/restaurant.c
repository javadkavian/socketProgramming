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




typedef struct {
    char name[100];
    struct {
        char ingredient[100];
        int quantity;
    } ingredients[MAX_INGREDIENT_COUNT];
    int ingredientCount;
} Food;


typedef struct restaurant{
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
    int status;
}restaurant;


void config_restaurant(restaurant* restaurant_, int argc, char* argv[]){
    if(argc != 2){
        logError("port not provided");
    }
    //udp-------------
    if((restaurant_ -> UDP_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        logError("socket");
    }
    int broadcast = 1;
    int opt = 1;
    restaurant_ -> UDP_port = atoi(argv[1]);
    setsockopt(restaurant_ -> UDP_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    setsockopt(restaurant_ -> UDP_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    restaurant_ -> UDP_address.sin_family = AF_INET; 
    restaurant_ -> UDP_address.sin_port = htons(restaurant_ -> UDP_port); 
    restaurant_ -> UDP_address.sin_addr.s_addr = inet_addr("10.0.2.255");
    if((bind(restaurant_ -> UDP_fd, (struct sockaddr *)&restaurant_ -> UDP_address, sizeof(restaurant_ -> UDP_address))) < 0){
        logError("bind");
    }
    //udp--------------
    //tcp-------------
    restaurant_ -> TCP_port = generate_random_port();
    if((restaurant_ -> server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        logError("socket");
    }
    int opt_ = 1;
    setsockopt(restaurant_ -> server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_, sizeof(opt_));
    restaurant_ -> TCP_address.sin_family = AF_INET;
    restaurant_ -> TCP_address.sin_addr.s_addr = INADDR_ANY;
    restaurant_ -> TCP_address.sin_port = htons(restaurant_ -> TCP_port);
    while((bind(restaurant_ -> server_fd, (struct sockaddr *)&restaurant_ -> TCP_address, sizeof(restaurant_ -> TCP_address))) < 0){
        restaurant_ -> TCP_port = generate_random_port();
        restaurant_ -> TCP_address.sin_port = htons(restaurant_ -> TCP_port);
    }
    if(listen(restaurant_ -> server_fd, 20) < 0){
        logError("listen");
    }
    memset(restaurant_ -> user_name, 0, MAX_NAME_SIZE);
    restaurant_ -> status = OPEN;
}



void send_message(restaurant* restaurant_, char* message){
    sendto(restaurant_ -> UDP_fd, message, strlen(message), 0,(struct sockaddr *)&restaurant_ -> UDP_address, sizeof(restaurant_ -> UDP_address));
}


void username_check(restaurant* restaurant_){
    char username[MAX_NAME_SIZE];
    memset(username, 0, MAX_NAME_SIZE);
    write(STDOUT_FILENO, ">>please enter your username:", 30);
    int n = read(STDIN_FILENO, username, MAX_NAME_SIZE);
    username[n-1] = '\0';
    strcpy(restaurant_ -> user_name, username);
    memset(restaurant_ -> buf, 0, BUFFER_SIZE);
    sprintf(restaurant_ -> buf, "%s|%d|%s|", USERNAMECHECK, restaurant_ -> TCP_port, username);
    send_message(restaurant_, restaurant_ -> buf);
    fd_set tmp;
    FD_ZERO(&tmp);
    FD_SET(restaurant_ -> server_fd, &tmp);
    int max_fd = restaurant_ -> server_fd;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    while(1){
        if(select(max_fd + 1, &tmp, NULL, NULL, &tv) < 0){
            logError("select");
        }
        if(FD_ISSET(restaurant_ -> server_fd, &tmp)){
            int new_socket = acceptClient(restaurant_ -> server_fd);
            char tmp_buf[BUF_SIZE];
            memset(tmp_buf, 0, BUF_SIZE);
            recv(new_socket, tmp_buf, BUF_SIZE, 0);
            char* command = strtok(tmp_buf, DELIM);
            if(strcmp(command, USERNAMEDENIED) == 0){
                write(STDOUT_FILENO, ">>username already exists!please try again>>", 44);
                memset(username, 0, MAX_NAME_SIZE);
                int x = read(STDIN_FILENO, username, MAX_NAME_SIZE);
                username[n-1] = '\0';
                strcpy(restaurant_ -> user_name, username);
                memset(restaurant_ -> buf, 0, BUFFER_SIZE);
                sprintf(restaurant_ -> buf, "%s|%d|%s|", USERNAMECHECK, restaurant_ -> TCP_port, username);
                send_message(restaurant_, restaurant_ -> buf);
            }
        }
        else{
            char tmp_message [BUFFER_SIZE];
            memset(tmp_message, 0, BUF_SIZE);
            sprintf(tmp_message, ">>welcome %s as a restaurant\n", restaurant_ -> user_name);
            write(STDOUT_FILENO, tmp_message, BUFFER_SIZE);
            return;
        }
    }
}



void handle_command(restaurant* restaurant_, char* input_line){
    char tmp_buf[BUF_SIZE];
    memset(tmp_buf, 0, BUF_SIZE);
    char* command = strtok(input_line, DELIM);
    if(strcmp(command, USERNAMECHECK) == 0){
        char* port_num_str = strtok(NULL, DELIM);
        int port = atoi(port_num_str);
        char* tmp_name = strtok(NULL, DELIM);
        if(port != restaurant_ -> TCP_port){
            if(strcmp(tmp_name, restaurant_ -> user_name) == 0){
                sprintf(restaurant_ -> buf, "%s|", USERNAMEDENIED);
                int fd = connectServer(port);
                send(fd, restaurant_ -> buf, BUFFER_SIZE, 0);
            }
            
        }
    }
    else if(strcmp(command, START_WORKING) == 0){
        restaurant_ -> status = OPEN;
        char tmp_message[BUF_SIZE];
        memset(tmp_message, 0, BUF_SIZE);
        sprintf(tmp_message, "%s|%s restaurant opened!|", ANNOUNCE_OPEN_RESTAURANT, restaurant_ -> user_name);
        send_message(restaurant_, tmp_message);
    }
    else if(strcmp(command, BREAK) == 0){
        //check pending orders
        restaurant_ -> status = CLOSE;
    }
}




void run_restaurant(restaurant* restaurant_){
    FD_ZERO(&restaurant_ -> master_set);
    restaurant_ -> max_fd = restaurant_ -> server_fd;
    FD_SET(restaurant_ -> server_fd, &restaurant_ -> master_set);
    FD_SET(restaurant_ -> UDP_fd, &restaurant_ -> master_set);
    FD_SET(STDIN_FILENO, &restaurant_ -> master_set);
    fd_set working_set;
    int iteration = 1;
    while (1)
    {
        working_set = restaurant_ -> master_set;
        if(select(restaurant_ -> max_fd + 1, &working_set, NULL, NULL, NULL) < 0){
            logError("select");
        }
        for(int i = 0 ; i <= restaurant_ -> max_fd ; i++){
            if(FD_ISSET(i, &working_set)){
                if(i == STDIN_FILENO){
                    memset(restaurant_ -> buf, 0, BUFFER_SIZE);
                    int x = read(STDIN_FILENO, restaurant_ -> buf, BUFFER_SIZE);
                    restaurant_ -> buf[x - 1] = '\0';
                    concatenate_string(restaurant_ -> buf, "|");
                    handle_command(restaurant_, restaurant_ -> buf);

                }
                else if(i == restaurant_ -> server_fd){
                    int new_socket = acceptClient(restaurant_ -> server_fd);
                    FD_SET(new_socket, &restaurant_ -> master_set);
                    if(new_socket > restaurant_ -> max_fd){
                        restaurant_ -> max_fd = new_socket;
                    }
                }
                else if(i == restaurant_ -> UDP_fd){
                    memset(restaurant_ -> buf, 0, BUFFER_SIZE);
                    recv(restaurant_ -> UDP_fd, restaurant_ -> buf, BUFFER_SIZE, 0);
                    handle_command(restaurant_, restaurant_ -> buf);
                }
                else{
                    memset(restaurant_ -> buf, 0, BUFFER_SIZE);
                    recv(i, restaurant_ -> buf, BUFFER_SIZE, 0);
                    handle_command(restaurant_, restaurant_ -> buf);
                }
            }
        }
    }
    
}




int main(int argc, char* argv[]){
    restaurant restaurant_;
    config_restaurant(&restaurant_, argc, argv);
    username_check(&restaurant_);
    run_restaurant(&restaurant_);
}