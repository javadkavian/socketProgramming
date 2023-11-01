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
#include <termios.h>
#include <unistd.h>

typedef struct order{
    int port;
    char food_name[MAX_DISH_NAME_LENGTH];
    int active;
    int accept;
    char customer_name[MAX_NAME_SIZE];
}order;

typedef struct ingredient{
    char ingredient[100];
    int quantity;
}ingredient;



typedef struct Food{
    char name[100];
    ingredient ingredients[MAX_INGREDIENT_COUNT];
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
    Food foods[MAX_FOOD_COUNT];
    int foodCount;
    ingredient ingredients[NUMBER_OF_INGREDIENT];
    int ingredientCount;
    order orders[MAX_ORDER_NUMS];
    int order_count;
}restaurant;


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


void parseJSON(restaurant* rest) {
    FILE* file = fopen("./src/recipes.json", "r");
    if (file == NULL) {
        printf("Failed to open the JSON file.\n");
        return;
    }
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* jsonData = (char*)malloc(fileSize + 1);
    fread(jsonData, 1, fileSize, file);
    fclose(file);
    jsonData[fileSize] = '\0';
    cJSON* root = cJSON_Parse(jsonData);
    free(jsonData);
    if (root == NULL) {
        printf("Failed to parse JSON data.\n");
        return;
    }
    cJSON* foodItem = root->child;
    int foodCount = 0;

    while (foodItem != NULL && foodCount < MAX_FOOD_COUNT) {
        const char* foodName = foodItem->string;
        cJSON* ingredient = foodItem->child;
        int ingredientCount = 0;
        while (ingredient != NULL && ingredientCount < MAX_INGREDIENT_COUNT) {
            const char* ingredientName = ingredient->string;
            int quantity = ingredient->valueint;
            strcpy(rest->foods[foodCount].ingredients[ingredientCount].ingredient, ingredientName);
            rest->foods[foodCount].ingredients[ingredientCount].quantity = quantity;
            ingredientCount++;

            ingredient = ingredient->next;
        }

        strcpy(rest->foods[foodCount].name, foodName);
        rest->foods[foodCount].ingredientCount = ingredientCount;

        foodCount++;
        foodItem = foodItem->next;
    }

    cJSON_Delete(root);

    rest->foodCount = foodCount;
}




void fillIngredientsArray(restaurant* rest) {
    memset(rest->ingredients, 0, sizeof(rest->ingredients));
    int ingredientCount = 0;
    for (int i = 0; i < rest->foodCount; i++) {
        Food* food = &(rest->foods[i]);
        for (int j = 0; j < food->ingredientCount; j++) {
            ingredient* foodIngredient = &(food->ingredients[j]);
            int found = 0;
            for (int k = 0; k < NUMBER_OF_INGREDIENT; k++) {
                ingredient* restIngredient = &(rest->ingredients[k]);
                if (strcmp(foodIngredient->ingredient, restIngredient->ingredient) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                for (int k = 0; k < NUMBER_OF_INGREDIENT; k++) {
                    ingredient* restIngredient = &(rest->ingredients[k]);
                    if (strlen(restIngredient->ingredient) == 0) {
                        strcpy(restIngredient->ingredient, foodIngredient->ingredient);
                        restIngredient->quantity = INITIAL_ING_VALUE;
                        ingredientCount++;
                        break;
                    }
                }
            }
        }
    }
    rest->ingredientCount = ingredientCount;
}







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
    restaurant_ -> order_count = 0;
    restaurant_ -> status = OPEN;
    parseJSON(restaurant_);
    fillIngredientsArray(restaurant_);
    write(STDOUT_FILENO, ">>please enter your username:", 30);
    
}



void send_message(restaurant* restaurant_, char* message){
    sendto(restaurant_ -> UDP_fd, message, strlen(message), 0,(struct sockaddr *)&restaurant_ -> UDP_address, sizeof(restaurant_ -> UDP_address));
}


void username_check(restaurant* restaurant_){
    char username[MAX_NAME_SIZE];
    memset(username, 0, MAX_NAME_SIZE);
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
            close(new_socket);
            FD_CLR(new_socket, &tmp);
            char* command = strtok(tmp_buf, DELIM);
            if(strcmp(command, USERNAMEDENIED) == 0){
                write(STDOUT_FILENO, ">>username already exists!please try again>>", 44);
                username_check(restaurant_);
                return;
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

int find_order_by_port(restaurant* restaurant_, int port){
    for(int i = 0 ; i < restaurant_ -> order_count ; i++){
        if(restaurant_ -> orders[i].port == port){
            return i;
        }
    }
    return -1;
}



int enough_by_number(restaurant* restaurant_, int x, char* ing_name){
    for(int i = 0 ; i < restaurant_ -> ingredientCount ; i++){
        if(strcmp(restaurant_ -> ingredients[i].ingredient, ing_name) == 0){
            if(restaurant_ -> ingredients[i].quantity >= x){
                return 1;
            }
            else{
                return 0;
            }
        }
    }
}


int supply_for_food_available(restaurant* restaurant_, int index){
    int ans = 1;
    for(int i = 0 ; i < restaurant_ -> foodCount ; i++){
        if(strcmp(restaurant_ -> foods[i].name, restaurant_ -> orders[index].food_name) == 0){
            for(int j = 0; j < restaurant_ -> foods[i].ingredientCount ; j++){
                if(enough_by_number(restaurant_, restaurant_->foods[i].ingredients[j].quantity,
                 restaurant_ -> foods[i].ingredients[j].ingredient) == 0){
                    ans = 0;
                }
            }            
        }
    }
    return ans;
}


void add_ing(restaurant* restaurant_, char* ing, int qunt){
    for(int i = 0 ; i < restaurant_ -> ingredientCount ; i++){
        if(strcmp(ing, restaurant_ -> ingredients[i].ingredient) == 0){
            restaurant_ -> ingredients[i].quantity += qunt;
        }
    }
}

void handle_command(restaurant* restaurant_, char* input_line){
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
                close(fd);
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
        int flag = 0;
        for(int i = 0 ; i < restaurant_ -> order_count ; i++){
            if(restaurant_ -> orders[i].active == 1){
                flag = 1;
            }
        }
        if(flag == 0){
            restaurant_ -> status = CLOSE;
            char tmp_msg[BUF_SIZE];
            memset(tmp_msg, 0, BUF_SIZE);
            sprintf(tmp_msg, "%s|%s|", REST_CLOSED, restaurant_ -> user_name);
        }
    }
    else if(strcmp(command, SHOW_RESTAURANTS_) == 0){
        char tmp_msg[BUF_SIZE];
        memset(tmp_msg, 0, BUF_SIZE);
        sprintf(tmp_msg, "%s|%s|%d|", I_AM_RESTAURANT, restaurant_ -> user_name, restaurant_ -> TCP_port);
        char*port_num_str = strtok(NULL, DELIM);
        int port = atoi(port_num_str);
        int fd = connectServer(port);
        send(fd, tmp_msg, BUFFER_SIZE, 0);
        close(fd);
    }
    else if(strcmp(command, ORDER_FOOD_R) == 0){
        write(STDOUT_FILENO, "new order!\n", 12);
        char* port_str = strtok(NULL, DELIM);
        int port = atoi(port_str);
        char* food_name = strtok(NULL, DELIM);
        char* customer_name = strtok(NULL, DELIM);
        strcpy(restaurant_ -> orders[restaurant_ -> order_count].customer_name, customer_name);
        strcpy(restaurant_ -> orders[restaurant_ -> order_count].food_name, food_name);
        restaurant_ -> orders[restaurant_ -> order_count].accept = 0;
        restaurant_ -> orders[restaurant_ -> order_count].active = 1;
        restaurant_ -> orders[restaurant_ -> order_count].port = port;
        restaurant_ -> order_count += 1;
        
        // write(STDOUT_FILENO, "order taken\n", 12); log kon badan
    }
    else if(strcmp(command, ORDER_EXPIRD) == 0){
        char* port_str = strtok(NULL, DELIM);
        int port = atoi(port_str);
        for(int i = 0 ; i < restaurant_ -> order_count ; i++){
            if(restaurant_ -> orders[i].port == port){
                restaurant_ -> orders[i].active = 0;
                restaurant_ -> orders[i].accept = 0;
                //order expired
            }

        }
    }
    else if(strcmp(command, SHOW_REST_REQUESTS) == 0){
        write(STDOUT_FILENO, "username/port/order\n", 21);
        char tmp_msg[BUF_SIZE];
        for(int i = 0 ; i < restaurant_ -> order_count ; i++){
            if(restaurant_ -> orders[i].active == 1){
                memset(tmp_msg, 0, BUF_SIZE);
                sprintf(tmp_msg, "%s %d %s\n",
                restaurant_ -> orders[i].customer_name,
                restaurant_ ->orders[i].port, restaurant_ -> orders[i].food_name);
                write(STDOUT_FILENO, tmp_msg, BUF_SIZE);
            }
        }
    }
    else if(strcmp(command, ANSWER_REQ) == 0){
        write(STDOUT_FILENO, ">> port of request : ", 22);
        char port_str[10];
        memset(port_str, 0, 10);
        int n = read(STDIN_FILENO, port_str, 10);
        port_str[n-1] = '\0';
        int port = atoi(port_str);
        int index = find_order_by_port(restaurant_, port);
        write(STDOUT_FILENO, "your answer (YES/NO): ", 22);
        char ans[10];
        memset(ans, 0, 10);
        int x = read(STDIN_FILENO, ans, 10);
        char msg[BUF_SIZE];
        memset(msg, 0, BUF_SIZE);
        if((supply_for_food_available(restaurant_, index) == 1) && (strcmp(ans, "YES") == 0)){
            sprintf(msg, "%s|%s|", FOOD_ACCEPTED, restaurant_ -> user_name);
            restaurant_ -> orders[index].accept = 1;

        }
        else{
            sprintf(msg, "%s|%s|", FOOD_REJECTED, restaurant_ -> user_name);
            restaurant_ -> orders[index].accept = 0;
        }
        int fd = connectServer(port);
        send(fd, msg, BUF_SIZE, 0);
        close(fd);
    }
    else if(strcmp(command, SHOW_SALE_HISTORY) == 0){
        write(STDOUT_FILENO, "username/order/result\n", 23);
        char tmp_msg[BUF_SIZE];
        for(int i = 0 ; i < restaurant_ -> order_count ; i++){
            memset(tmp_msg, 0, BUF_SIZE);
            if(restaurant_ -> orders[i].accept == 1){
                sprintf(tmp_msg, "%s %s accepted\n", restaurant_ -> orders[i].customer_name, restaurant_ -> orders[i].food_name);
            }
            else{
                sprintf(tmp_msg, "%s %s denied\n", restaurant_ -> orders[i].customer_name, restaurant_ -> orders[i].food_name);
            }
            write(STDOUT_FILENO, tmp_msg, BUF_SIZE);
        }
    }
    else if(strcmp(command, SHOW_SUPPLYERS_R) == 0){
        char msg[BUF_SIZE];
        memset(msg, 0, BUF_SIZE);
        sprintf(msg, "%s|%d|", SHOW_SUPPLYERS_S, restaurant_ -> TCP_port);
        send_message(restaurant_, msg);
    }
    else if(strcmp(command, I_AM_SUPPLYAER) == 0){
        char* supplyer_name = strtok(NULL, DELIM);
        char* port_str = strtok(NULL, DELIM);
        char msg [BUF_SIZE];
        memset(msg, 0, BUF_SIZE);
        sprintf(msg, "%s %s\n", supplyer_name, port_str);
        write(STDOUT_FILENO, msg, BUF_SIZE);
    }
    else if(strcmp(command, REQ_ING_R) == 0){
        write(STDOUT_FILENO, ">>port of supplier : ", 22);
        char port_str[6];
        memset(port_str, 0, 6);
        int x = read(STDIN_FILENO, port_str, 6);
        port_str[x-1] = '\0';
        int port = atoi(port_str);
        write(STDOUT_FILENO, ">>name of ingedient : ", 23);
        char ing_name[MAX_NAME_SIZE];
        memset(ing_name, 0, MAX_NAME_SIZE);
        int n = read(STDIN_FILENO, ing_name, MAX_NAME_SIZE);
        ing_name[n-1] = '\0';
        write(STDOUT_FILENO, ">>quantity of ingeredient : ", 29);
        char quantity[20];
        memset(quantity, 0, 20);
        int y = read(STDIN_FILENO, quantity, 20);
        quantity[y-1] = '\0';
        char tmp_msg[BUF_SIZE];
        memset(tmp_msg, 0, BUF_SIZE);
        sprintf(tmp_msg, "%s|%d|%s|%s|%s|", REQ_ING_S, restaurant_ -> TCP_port, ing_name, quantity, restaurant_ -> user_name);
        int fd = connectServer(port);
        send(fd, tmp_msg, BUF_SIZE, 0);
        close(fd);
        fd_set tmp_fd_set;
        FD_ZERO(&tmp_fd_set);
        FD_SET(restaurant_ -> server_fd, &tmp_fd_set);
        int max_fd = restaurant_ -> server_fd;
        struct timeval tv;
        tv.tv_sec = 90;
        tv.tv_usec = 0;
        write(STDOUT_FILENO, "waiting for supplier's response...\n", 36);
        enableRawMode();
        while(1){
            if(select(max_fd + 1, &tmp_fd_set, 0, 0, &tv) < 0){
                logError("select");
            }
            if(FD_ISSET(restaurant_ -> server_fd, &tmp_fd_set)){
                int new_sock = acceptClient(restaurant_ -> server_fd);
                char recv_msg[BUF_SIZE];
                memset(recv_msg, 0, BUF_SIZE);
                recv(new_sock, recv_msg, BUF_SIZE, 0);
                char disp_msg[BUF_SIZE];
                memset(disp_msg, 0, BUF_SIZE);
                char* ans = strtok(recv_msg, DELIM);
                char* sup_name = strtok(NULL, DELIM);
                if(strcmp(ans, ING_ACC) == 0){
                    sprintf(disp_msg, "%s supplier accepted your order and your ingredient is ready\n", sup_name);
                    add_ing(restaurant_, ing_name, atoi(quantity));
                }
                else if(strcmp(ans, BUSY) == 0){
                    sprintf(disp_msg, "%s is now busy\n", sup_name);
                }
                else{
                    sprintf(disp_msg, "%s supplier rejected your order\n", sup_name);
                }
                write(STDOUT_FILENO, disp_msg, BUF_SIZE);
                break;
            }
            else{
                logInfo("timeOut");
                char msg[BUF_SIZE];
                memset(msg, 0, BUF_SIZE);
                sprintf(msg, "%s|%d|", ORDER_EXPIRD, restaurant_ -> TCP_port);
                int fd_ = connectServer(port);
                send(fd_, msg, BUF_SIZE, 0);
                close(fd);
                break;
            }
        }
        disableRawMode();
    }


}




void run_restaurant(restaurant* restaurant_){
    FD_ZERO(&restaurant_ -> master_set);
    restaurant_ -> max_fd = restaurant_ -> server_fd;
    FD_SET(restaurant_ -> server_fd, &restaurant_ -> master_set);
    FD_SET(restaurant_ -> UDP_fd, &restaurant_ -> master_set);
    FD_SET(STDIN_FILENO, &restaurant_ -> master_set);
    fd_set working_set;
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
                    close(i);
                    FD_CLR(i, &restaurant_ -> master_set);
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







