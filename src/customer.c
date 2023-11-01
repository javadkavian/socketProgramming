#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "include/cJSON.h"
#include <unistd.h>
#include "include/user.h"



char* getDishNamesFromFile() {
    FILE* file = fopen("./src/recipes.json", "r");
    if (file == NULL) {
        printf("Failed to open the file.\n");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* jsonData = (char*)malloc(fileSize + 1);
    if (jsonData == NULL) {
        printf("Memory allocation failed.\n");
        fclose(file);
        return NULL;
    }
    fread(jsonData, 1, fileSize, file);
    fclose(file);
    jsonData[fileSize] = '\0';
    cJSON* root = cJSON_Parse(jsonData);
    if (root == NULL) {
        printf("Failed to parse JSON data.\n");
        free(jsonData);
        return NULL;
    }

    cJSON* dishObj = NULL;
    int count = 0;
    char* dishNames = (char*)malloc(MAX_DISHES * (MAX_DISH_NAME_LENGTH + 5) * sizeof(char));
    if (dishNames == NULL) {
        printf("Memory allocation failed.\n");
        cJSON_Delete(root);
        free(jsonData);
        return NULL;
    }

    cJSON_ArrayForEach(dishObj, root) {
        const char* dishName = dishObj->string;
        snprintf(dishNames + (count * (MAX_DISH_NAME_LENGTH + 5)), MAX_DISH_NAME_LENGTH + 5, "%d.%s\n", count + 1, dishName);
        count++;
        if (count >= MAX_DISHES) {
            break;
        }
    }
    cJSON_Delete(root);
    free(jsonData);
    return dishNames;
}



int main(){
    char* dishnames = getDishNamesFromFile();
    write(STDOUT_FILENO, dishnames, 2000);
}