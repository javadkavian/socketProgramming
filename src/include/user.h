#ifndef __USER__
#define __USER__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define MAX_PORT_NUMBER 50000
#define MIN_PORT_NUMBER 5000
#define BUFFER_SIZE 1024
#define MAX_NAME_SIZE 50
#define USERNAMECHECK "usernameCheck"
#define DELIM "|"
#define USERNAMEGRANT "usernameGrant"
#define USERNAMEDENIED "usernameDenied"
#define OPEN 1
#define CLOSE 0
#define START_WORKING "start working"
#define ANNOUNCE_OPEN_RESTAURANT "arop"
#define BREAK "break"
#define MAX_FOOD_COUNT 20
#define MAX_INGREDIENT_COUNT 10
#define NUMBER_OF_INGREDIENT 20
#define INITIAL_ING_VALUE 1000
#define MAX_DISHES 20
#define MAX_DISH_NAME_LENGTH 20
#define SHOW_MENU "show menu"
#define SHOW_RESTAURANTS "show restaurants"
#define SHOW_RESTAURANTS_ "show restaurants_"
#define I_AM_RESTAURANT "I am restaurant"
#define ORDER_FOOD_C "order food"
#define ORDER_FOOD_R "order food_r"
#define ANSWER_REQ "answer request"
#define MAX_ORDER_NUMS 50
#define ORDER_EXPIRD "order expired"
#define SHOW_REST_REQUESTS "show requests list"

int acceptClient(int server_fd);
int generate_random_port();
int connectServer(int port);
void concatenate_string(char* s, char* s1);




#endif