
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fatFileSystem.h"
#include <pthread.h>
#include <time.h>

#define COMM_COUNT 10
char command[COMM_COUNT][100] = { "mkdir", "rmdir", "cd", "pwd", "ls", "clear", "touch", "exit", "rm" };
size_t getCommand(char* str);
void listDirectory(char *directoryPath);
void *mt_createFile(void *filePath);
void *mt_deleteFile(void *filePath);
char** parsInput(char* str);

int main(int argc, const char * argv[]) {

    system("clear");

    format();
    readEncryptedDisk("virtualdisk_encrypted", "04060755qW123");

    int fl = 1;
    MyFILE * file;

    char* inp = (char*)calloc(100,sizeof(char));
    while(fl){
        fprintf(stdout, "user@name:");
        pwd();
        gets(inp);
        char** buf = parsInput(inp);
        size_t command_inp = getCommand(buf[0]);
        switch (command_inp)
        {
        case 0:{
            mymkdir(buf[1]);
            mychdir(buf[1]);
            break;
        }
        case 1:{
            myrmdir(buf[1]);
            break;
        }
        case 2:{
            mychdir(buf[1]);
            break;
        }
        case 3:{
            pwd();
            fprintf(stdout, "\n");
            break;
        }
        case 4:{
            listDirectory(".");
            break;
        }
        case 5:{
            system("clear");
            break;
        }
        case 6:{
            file = myfopen(buf[1], "w");
            myfclose(file);
            break;
        }
        case 7: {
            fl = 0;
            writeEncryptedDisk("virtualdisk_encrypted", "04060755qW123");
            break;
        }
        case 8:{
            myremove(buf[1]);
            break;
        }
        case 9:{
            break;
        }
        case __SIZE_MAX__:{
            fprintf(stdout, "%s: command is not found\n", buf[0]);
            break;
        }
        default:
            break;
        }
    }
    return 0;

    
}

char** parsInput(char* str){
    size_t count = 0;
    char** res = (char**)calloc(20,sizeof(char*));
    for(size_t i = 0; i < 20; ++i){
        res[i] = (char*)calloc(100,sizeof(char));
    }

    size_t buf_ptr = 0;
    for(size_t i = 0; i < strlen(str); ++i){
        if(str[i] == '\n'){
            break;
        }
        if(str[i] == ' '){
            buf_ptr = 0;
            ++count;
            continue;
        }
        res[count][buf_ptr++] = str[i];
    }
    return res;
}

size_t getCommand(char* str){
    size_t i = 0;
    for(i; i < COMM_COUNT; ++i){
        if(strcmp(command[i], str) == 0){
            return i;
        }
    }

    i = __SIZE_MAX__;
    return i;
}


void *mt_createFile(void *filePath) {
    pthread_mutex_lock(getVirtualDiskLock());
    MyFILE *file = myfopen(filePath, "w");
    myfputc('X', file);
    myfclose(file);
    pthread_mutex_unlock(getVirtualDiskLock());
    pthread_exit(NULL);
}

void *mt_deleteFile(void *filePath) {
    pthread_mutex_lock(getVirtualDiskLock());
    myremove(filePath);
    pthread_mutex_unlock(getVirtualDiskLock());
    pthread_exit(NULL);
}

void listDirectory(char *directoryPath) {
    char **directoryEntries = mylistdir(directoryPath);
    if(directoryEntries != NULL) {
        int i = 0;
        while(directoryEntries[i] != NULL) {
            printf("%s \n", directoryEntries[i]);
            free(directoryEntries[i]);
            i++;
        }
    } else {
        printf("Directory is empty\n");
    }
}