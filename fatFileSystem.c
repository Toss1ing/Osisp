#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fatFileSystem.h"

diskblock_t virtualDisk[MAXBLOCKS];
fatentry_t  FAT[MAXBLOCKS];

dirblock_t *rootDirectoryBlock = &(virtualDisk[rootDirectoryIndex].dir);
direntry_t *currentDir = NULL;
fatentry_t currentDirIndex = 0;

void writedisk(const char *filename) {
   printf("writedisk> virtualdisk[0] = %s\n", virtualDisk[0].data);
   FILE *dest = fopen(filename, "w");
   if (fwrite(virtualDisk, sizeof(virtualDisk), 1, dest ) < 1)
      fprintf(stderr, "write virtual disk to disk failed\n");
   fclose(dest) ;
}

void readdisk(const char *filename) {
    FILE *dest = fopen(filename, "r");
    if (fread(virtualDisk, sizeof(virtualDisk), 1, dest) < 1)
        fprintf(stderr, "write virtual disk to disk failed\n");
    else
        loadFAT();
    fclose(dest) ;
}

void writeEncryptedDisk(const char *filename, const char *password) {
    unsigned long keyLength = strlen(password);
    diskblock_t encryptedDisk [MAXBLOCKS];
    memset(encryptedDisk, 0, sizeof(encryptedDisk));
    for(int i = 0; i < MAXBLOCKS; i++) {
        memcpy(encryptedDisk[i].data, virtualDisk[i].data, BLOCKSIZE);
    }

    for(int i = 0; i < MAXBLOCKS; i++) {
        for(int j = 0; j < BLOCKSIZE; j++) {
            if(encryptedDisk[i].data[j] != 0xff &&
               encryptedDisk[i].data[j] != 0 &&
               encryptedDisk[i].data[j] != password[j % keyLength]) {
                encryptedDisk[i].data[j] ^= (password[j % keyLength]);
            }
        }
    }
    
    FILE *encryptedFile = fopen(filename, "w");
    fwrite(encryptedDisk, sizeof(encryptedDisk), 1, encryptedFile);
    fclose(encryptedFile);
}

void readEncryptedDisk(const char *filename, const char *password) {
    FILE *encryptedFile = fopen(filename, "r");
    unsigned long keyLength = strlen(password);
    diskblock_t encryptedDisk [MAXBLOCKS];
    fread(encryptedDisk, sizeof(encryptedDisk), 1, encryptedFile);
    memset(virtualDisk, 0, sizeof(encryptedDisk));
    
    for(size_t i = 0; i < MAXBLOCKS; i++) {
        for(size_t j = 0; j < BLOCKSIZE; j++) {
            if(encryptedDisk[i].data[j] != 0xff &&
               encryptedDisk[i].data[j] != 0 &&
               encryptedDisk[i].data[j] != password[j % keyLength]) {
                encryptedDisk[i].data[j] ^= (password[j % keyLength]);
            }
        }
    }
    
    for(size_t i = 0; i < MAXBLOCKS; i++) {
        memcpy(virtualDisk[i].data, encryptedDisk[i].data, BLOCKSIZE);
    }
    fclose(encryptedFile);
}

void writeblock(diskblock_t *block, int block_address) {
   memmove(virtualDisk[block_address].data, block->data, BLOCKSIZE);
}

void format() {
    for(size_t i = 0; i < MAXBLOCKS; i++){
        memset(virtualDisk[i].data, 0, BLOCKSIZE);
    }
    
    diskblock_t block;

    memset(block.data, 0, BLOCKSIZE);
    strcpy(block.volume.name, "CS3026 Operating Systems Assessment Multi-Threaded");
    pthread_mutex_init(&(block.volume.lock), NULL);
    writeblock(&block, 0);

    memset(block.data, 0, BLOCKSIZE);
    block.dir.isdir = 1;
    block.dir.nextEntry = 0;
    writeblock(&block, 3);
    
    memset(FAT, UNUSED, MAXBLOCKS * 2);
    FAT[0] = ENDOFCHAIN;
    FAT[1] = 2;
    FAT[2] = ENDOFCHAIN;
    FAT[3] = ENDOFCHAIN;
    saveFAT();
}

pthread_mutex_t *getVirtualDiskLock(void) {
    return &virtualDisk[0].volume.lock;
}

void loadFAT() {
    memcpy(FAT, virtualDisk[1].fat, MAXBLOCKS);
    memcpy(FAT + FATENTRYCOUNT - 1, virtualDisk[2].fat, MAXBLOCKS);
}

// Save FAT to virtual disk
void saveFAT() {
    memcpy(virtualDisk[1].fat, FAT, MAXBLOCKS);
    memcpy(virtualDisk[2].fat, FAT + FATENTRYCOUNT - 1, MAXBLOCKS);
}

int freeFAT() {
    for(size_t i = 0; i < MAXBLOCKS; i++) {
        if(FAT[i] == UNUSED) return i;
    }
    return -1;
}


static void myfopenRead(const char *filePath, MyFILE **file) {
    char *fileName = NULL;
    dirblock_t *directoryBlock = findDirectoryBlock(filePath, &fileName, 0);
    if(directoryBlock == NULL || fileName == NULL) {
        *file = NULL;
        return;
    }
    
    int foundFile = 0;
    for(int i = 0; i < directoryBlock->nextEntry; i++) {
        if(strcmp(directoryBlock->entrylist[i].name, fileName) == 0 &&
           !directoryBlock->entrylist[i].isdir) {
                (*file)->blockno = directoryBlock->entrylist[i].firstblock;
                (*file)->dirEntry = &directoryBlock->entrylist[i];
                memcpy((*file)->buffer.data, virtualDisk[(*file)->blockno].data, BLOCKSIZE);
                foundFile = 1;
        }
    }
    
    if(!foundFile) *file = NULL;
}

static void myfopenWrite(const char *filePath, MyFILE **file) {
    char *fileName = NULL;
    dirblock_t *directoryBlock = findDirectoryBlock(filePath, &fileName, 1);
    
    if(directoryBlock == NULL) return;
    
    for(int i = 0; i < directoryBlock->nextEntry; i++) {
        if(strcmp(directoryBlock->entrylist[i].name, fileName) == 0 &&
           !directoryBlock->entrylist[i].isdir) {
                (*file)->blockno = directoryBlock->entrylist[i].firstblock;
                (*file)->dirEntry = &directoryBlock->entrylist[i];
                memcpy((*file)->buffer.data, virtualDisk[(*file)->blockno].data, BLOCKSIZE);
                return;
        }
    }
    
    int freeBlockIndex = freeFAT();
    (*file)->blockno = freeBlockIndex;
    FAT[freeBlockIndex] = ENDOFCHAIN;
    saveFAT();
    
    direntry_t *fileDirectory = malloc(sizeof(direntry_t));
    memset(fileDirectory, 0, sizeof(direntry_t));
    fileDirectory->filelength = 0;
    fileDirectory->isdir = 0;
    fileDirectory->firstblock = freeBlockIndex;
    strcpy(fileDirectory->name, fileName);

    directoryBlock->entrylist[directoryBlock->nextEntry] = *fileDirectory;
    (*file)->dirEntry = &directoryBlock->entrylist[directoryBlock->nextEntry];
    directoryBlock->nextEntry++;
}

MyFILE *myfopen(const char *filename, const char *mode) {
    MyFILE *file = malloc(sizeof(MyFILE));
    memset(file->buffer.data, 0, BLOCKSIZE);
    strcpy(file->mode, mode);
    file->pos = 0;
    file->currentBlock = 0;
    
    if(strcmp(mode, "r") == 0) myfopenRead(filename, &file);
    else if (strcmp(mode, "w") == 0) myfopenWrite(filename, &file);
    
    return file;
}

void myfclose(MyFILE *stream) {
    if(stream == NULL) return;
    writeblock(&stream->buffer, stream->blockno);
    free(stream);
}

void myfputc(int b, MyFILE *stream) {
    if(stream == NULL || strcmp(stream->mode, "r") == 0) return;
    
    stream->buffer.data[stream->pos] = b;
    stream->pos++;
    stream->dirEntry->filelength++;
    
    if(stream->pos == BLOCKSIZE - 1) {
        writeblock(&stream->buffer, stream->blockno);
        int freeBlockIndex = freeFAT();
        FAT[stream->blockno] = freeBlockIndex;
        FAT[freeBlockIndex] = ENDOFCHAIN;
        saveFAT();
        stream->blockno = freeBlockIndex;
        stream->pos = 0;
    }
}

int myfgetc(MyFILE *stream) {
    if(stream == NULL) return EOF;
    if((stream->currentBlock * BLOCKSIZE + stream->pos) == stream->dirEntry->filelength) return EOF;
    
    int charInt = stream->buffer.data[stream->pos];
    stream->pos++;
    
    if(stream->pos == BLOCKSIZE) {
        int nextBlock = FAT[stream->blockno];
        if(nextBlock == UNUSED) {
            return EOF;
        }
        memcpy(stream->buffer.data, virtualDisk[nextBlock].data, BLOCKSIZE);
        stream->blockno = nextBlock;
        stream->pos = 0;
        stream->currentBlock++;
    }

    return charInt;
}

void myremove(const char *path) {
    char *fileName = NULL;
    dirblock_t *directoryBlock = findDirectoryBlock(path, &fileName, 0);
    if(directoryBlock == NULL) {
        fprintf(stderr, "Couldn't find file directory, nothing was deleted\n");
        return;
    }
    
    int foundFile = 0;
    for(int i = 0; i < directoryBlock->nextEntry; i++) {
        if(strcmp(directoryBlock->entrylist[i].name, fileName) == 0 &&
           !directoryBlock->entrylist[i].isdir && !foundFile) {
            foundFile = 1;
            cleanVirtualDisk(directoryBlock->entrylist[i].firstblock);
            memset(&directoryBlock->entrylist[i], 0, sizeof(direntry_t));
        }
        
        if(foundFile && i + 1 < directoryBlock->nextEntry) {
            memcpy(&directoryBlock->entrylist[i], &directoryBlock->entrylist[i + 1], sizeof(direntry_t));
            memset(&directoryBlock->entrylist[i + 1], 0, sizeof(direntry_t));
        }
    }
    
    if(!foundFile) {
        fprintf(stderr, "Couldn't find file, nothing was deleted\n");
        return;
    }
    directoryBlock->nextEntry--;
}

void copyToVirtualDisk(const char *virtualDiskPath, const char *realDiskPath) {
    MyFILE *vdFile = myfopen(virtualDiskPath, "w");
    FILE *rdFile = fopen(realDiskPath, "r");
    
    if(rdFile == NULL) {
        fprintf(stderr, "Couldn't find %s on real disk", realDiskPath);
        return;
    }
    
    if(vdFile == NULL) {
        fprintf(stderr, "Couldn't create file on virtual disk");
        return;
    }
    
    int character;
    while((character = fgetc(rdFile)) != EOF) myfputc(character, vdFile);
    
    myfclose(vdFile);
    fclose(rdFile);
}

void copyToRealDisk(const char *realDiskPath, const char *virtualDiskPath) {
    MyFILE *vdFile = myfopen(virtualDiskPath, "r");
    FILE *rdFile = fopen(realDiskPath, "w");
    
    if(rdFile == NULL) {
        fprintf(stderr, "Couldn't create file on real disk");
        return;
    }
    
    if(vdFile == NULL) {
        fprintf(stderr, "Couldn't find %s on virtual disk", virtualDiskPath);
        return;
    }
    
    int character;
    while((character = myfgetc(vdFile)) != EOF) fputc(character, rdFile);
    
    myfclose(vdFile);
    fclose(rdFile);
}

int copyFile(const char *source, const char *destination) {
    MyFILE *sourceFILE = myfopen(source, "r");
    MyFILE *destinationFILE = myfopen(destination, "w");
    if(sourceFILE == NULL || destinationFILE == NULL) {
        fprintf(stderr, "Operation failed, either destination directory is full, or source file doesn't exist\n");
        return 0;
    }
    
    int character;
    while((character = myfgetc(sourceFILE)) != EOF) {
        myfputc(character, destinationFILE);
    }
    myfclose(sourceFILE);
    myfclose(destinationFILE);
    return 1;
}

void moveFile(const char *source, const char *destination) {
    if(!copyFile(source, destination)) {
        fprintf(stderr, "File could not be moved\n");
        return;
    }
    myremove(source);
}

void cleanVirtualDisk(short firstFATIndex) {
    short currentFATIndex = firstFATIndex;
    while (1) {
        memset(virtualDisk[currentFATIndex].data, 0, BLOCKSIZE);\
        if(FAT[currentFATIndex] == ENDOFCHAIN) {
            FAT[currentFATIndex] = UNUSED;
            break;
        }
        int copyIndex = currentFATIndex;
        currentFATIndex = FAT[currentFATIndex];
        FAT[copyIndex] = UNUSED;
    }
    saveFAT();
}

dirblock_t * findDirectoryBlock(const char *path, char **filename, int modify) {
    char blockPath[MAXPATHLENGTH];
    strcpy(blockPath, path);
    
    dirblock_t *parentDirectoryBlock = NULL;
    int isAbsolute = blockPath[0] == '/';
    
    if(blockPath[0] == '.' && strlen(blockPath) == 1) {
        return &virtualDisk[currentDir->firstblock].dir;//currentDir->firstblock
    }
    if(blockPath[0] == '.' && blockPath[1] == '.' && strlen(blockPath) == 2) {
        if(currentDir == NULL) {
            printf("Couldn't find directory\n");
            return NULL;
        } else {
            dirblock_t *parentBlock = findParentBlock(rootDirectoryBlock, &virtualDisk[currentDir->firstblock].dir);
            if(parentBlock == rootDirectoryBlock) {
                printf("You are in the root directory");
                return NULL;
            }
            return parentBlock;
        }
    }
    
    if(isAbsolute || currentDir == NULL) parentDirectoryBlock = rootDirectoryBlock;
    else parentDirectoryBlock = &(virtualDisk[currentDir->firstblock].dir);
    
    char *head, *tail = blockPath;
    while ((head = strtok_r(tail, "/", &tail))) {
        if(strchr(head, '.') != NULL) {
            *filename = malloc(MAXNAME);
            strcpy(*filename, head);
            return parentDirectoryBlock;
        }
        
        int found = 0;
        for(int i = 0; i < parentDirectoryBlock->nextEntry; i++) {
            if(strcmp(parentDirectoryBlock->entrylist[i].name, head) == 0 &&
               parentDirectoryBlock->entrylist[i].isdir) {
                parentDirectoryBlock = &(virtualDisk[parentDirectoryBlock->entrylist[i].firstblock].dir);
                found = 1;
            }
        }
        
        if(found == 0 && modify == 0) return NULL;
        else if(found == 0 && modify) parentDirectoryBlock = createDirectoryBlock(parentDirectoryBlock, head);
    
    }
    return parentDirectoryBlock;
}

dirblock_t *getChildDirectoryBlock(dirblock_t *parentDirectoryBlock, const char *childDirectoryName) {
    for(int i = 0; i < parentDirectoryBlock->nextEntry; i++) {
        if(strcmp(parentDirectoryBlock->entrylist[i].name, childDirectoryName) == 0 &&
           parentDirectoryBlock->entrylist[i].isdir) {
            return &(virtualDisk[parentDirectoryBlock->entrylist[i].firstblock].dir);
        }
    }
    return NULL;
}

dirblock_t *createDirectoryBlock(dirblock_t *parentDirectoryBlock, const char *directoryName) {
    for(int i = 0; i < parentDirectoryBlock->nextEntry; i++) {
        if(strcmp(parentDirectoryBlock->entrylist[i].name, directoryName) == 0 &&
           parentDirectoryBlock->entrylist[i].isdir) {
            return &(virtualDisk[parentDirectoryBlock->entrylist[i].firstblock].dir);
        }
    }
    
    if(parentDirectoryBlock->nextEntry == DIRENTRYCOUNT) {
        fprintf(stderr, "Can't create child directory, because parent directory is full");
        return NULL;
    }

    int newDirectoryIndex = freeFAT();
    diskblock_t block;
    memset(block.data, 0, BLOCKSIZE);
    block.dir.isdir = 1;
    block.dir.nextEntry = 0;
    writeblock(&block, newDirectoryIndex);
    FAT[newDirectoryIndex] = ENDOFCHAIN;
    saveFAT();
    
    direntry_t directoryEntry;
    memset(&directoryEntry, 0, sizeof(direntry_t));
    directoryEntry.isdir = 1;
    directoryEntry.firstblock = newDirectoryIndex;
    strcpy(directoryEntry.name, directoryName);
    parentDirectoryBlock->entrylist[parentDirectoryBlock->nextEntry] = directoryEntry;
    parentDirectoryBlock->nextEntry++;
    return &virtualDisk[newDirectoryIndex].dir;
}

void mymkdir(const char *path) {
    char directoryPath[MAXPATHLENGTH];
    strcpy(directoryPath, path);
    int isAbsolute = directoryPath[0] == '/';
    
    dirblock_t *parent = NULL;
    if(isAbsolute || currentDir == NULL) {
        parent = rootDirectoryBlock;
    }
    else { 
        parent = &(virtualDisk[currentDir->firstblock].dir);
    }
    char *head;
    char *tail = directoryPath;
    while ((head = strtok_r(tail, "/", &tail))) {
        if(parent == NULL) {
            fprintf(stderr, "Can't create directory, parent directory either full or doesn't exist \n");
            break;
        }
        parent = createDirectoryBlock(parent, head);
    }
}

void myrmdir(const char *path) {
    dirblock_t *directoryBlock = findDirectoryBlock(path, NULL, 0);
    dirblock_t *parentDirectoryBlock = findParentBlock(rootDirectoryBlock, directoryBlock);
    
    if(directoryBlock == NULL) {
        fprintf(stderr, "Directory doesn't exist can't delete it");
        return;
    }

    deleteBlock(directoryBlock);
    int foundFolder = 0;
    for(int i = 0; i < parentDirectoryBlock->nextEntry; i++) {
        if(&virtualDisk[parentDirectoryBlock->entrylist[i].firstblock].dir == directoryBlock && !foundFolder) {
            foundFolder = 1;
            cleanVirtualDisk(parentDirectoryBlock->entrylist[i].firstblock);
            memset(&parentDirectoryBlock->entrylist[i], 0, sizeof(direntry_t));
        }
        
        if(foundFolder && i + 1 < parentDirectoryBlock->nextEntry) {
            memcpy(&parentDirectoryBlock->entrylist[i], &parentDirectoryBlock->entrylist[i + 1], sizeof(direntry_t));
            memset(&parentDirectoryBlock->entrylist[i + 1], 0, sizeof(direntry_t));
        }
    }
    
    if(!foundFolder) {
        fprintf(stderr, "Couldn't find folder, nothing was deleted\n");
        return;
    }

    parentDirectoryBlock->nextEntry--;
}

dirblock_t *findParentBlock(dirblock_t *startingBlock, dirblock_t *block) {
    for(int i = 0; i < startingBlock->nextEntry; i++) {
        if(startingBlock->entrylist[i].isdir) {
            if(&virtualDisk[startingBlock->entrylist[i].firstblock].dir == block) {
                return startingBlock;
            }
            dirblock_t *result = findParentBlock(&virtualDisk[startingBlock->entrylist[i].firstblock].dir, block);
            if(result != NULL) return result;
        }
    }
    return NULL;
}

void deleteBlock(dirblock_t *block) {
    for(int i = 0; i < block->nextEntry; i++) {
        if(block->entrylist[i].isdir) {
            deleteBlock(&virtualDisk[block->entrylist[i].firstblock].dir);
        }
        cleanVirtualDisk(block->entrylist[i].firstblock);
    }
}

char **mylistdir(const char *path) {
    dirblock_t *directoryBlock = findDirectoryBlock(path, NULL, 0);
    if(directoryBlock == NULL) {
        return NULL;
    }
    
    char **directoryEntries = malloc((DIRENTRYCOUNT + 1) * sizeof(char *));

    for(int i = 0; i < DIRENTRYCOUNT + 1; i++) {
        directoryEntries[i] = malloc(MAXNAME);
    }
    
    for(int i = 0; i < directoryBlock->nextEntry; i++) {
        strcpy(directoryEntries[i], directoryBlock->entrylist[i].name);
    }
    
    directoryEntries[directoryBlock->nextEntry] = NULL;
    return directoryEntries;
}


void mychdir(const char *path) {
    char directoryPath[MAXPATHLENGTH];
    strcpy(directoryPath, path);
    
    
    if(directoryPath[0] == '.' && strlen(directoryPath) == 1) return;
    if(directoryPath[0] == '.' && directoryPath[1] == '.' &&
       strlen(directoryPath) == 2) {
        if(currentDir == NULL) {
            return;
        } 
        else {
            dirblock_t *parentBlock = findParentBlock(rootDirectoryBlock, &virtualDisk[currentDir->firstblock].dir);
            if(parentBlock == rootDirectoryBlock) {
                currentDir = NULL;
                return;
            }
            dirblock_t *grandParentBlock = findParentBlock(rootDirectoryBlock, parentBlock);
            for(int i = 0; i < grandParentBlock->nextEntry; i++) {
                if(&virtualDisk[grandParentBlock->entrylist[i].firstblock].dir == parentBlock) {
                    currentDir = &grandParentBlock->entrylist[i];
                }
            }
        }
    }
    dirblock_t *parentDirectoryBlock = NULL;
    int isAbsolute = directoryPath[0] == '/';
    if(isAbsolute && strlen(directoryPath) == 1) {
        currentDir = NULL;
        return;
    }
    
    if(isAbsolute || currentDir == NULL) { 
        parentDirectoryBlock = rootDirectoryBlock;
    }
    else{
        parentDirectoryBlock = &(virtualDisk[currentDir->firstblock].dir);
    }
    
    char *head, *tail = directoryPath;
    while ((head = strtok_r(tail, "/", &tail))) {
        for(int i = 0; i < parentDirectoryBlock->nextEntry; i++) {
            if(strcmp(parentDirectoryBlock->entrylist[i].name, head) == 0 &&
               parentDirectoryBlock->entrylist[i].isdir) {
                currentDir = &parentDirectoryBlock->entrylist[i];
                parentDirectoryBlock = &(virtualDisk[parentDirectoryBlock->entrylist[i].firstblock].dir);
            }
        }
    }
}

static int pwdRec(dirblock_t *startingBlock, char **pathElements, int *elementCount) {
    for(int i = 0; i < startingBlock->nextEntry; i++) {
        if(&startingBlock->entrylist[i] == currentDir ||
           (startingBlock->entrylist[i].isdir && pwdRec(&virtualDisk[startingBlock->entrylist[i].firstblock].dir, pathElements, elementCount))) {
            pathElements[(*elementCount) - 1] = malloc(MAXNAME);
            memcpy(pathElements[(*elementCount) - 1], startingBlock->entrylist[i].name, MAXNAME);
            (*elementCount)++;
            pathElements = realloc(pathElements, *elementCount);
            return 1;
        }
    }
    return 0;
}

void pwd() {
    if(currentDir == NULL) { 
        return; 
    }
    else {
        int elementCount = 1;
        char **pathElements = malloc(elementCount * sizeof(char*));
        pwdRec(rootDirectoryBlock, pathElements, &elementCount);
        for(int i = elementCount - 2; i >= 0; i--) {
            printf("%s/", pathElements[i]);
            free(pathElements[i]);
        }
        free(pathElements);
    }
}