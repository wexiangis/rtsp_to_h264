
#ifndef _SHMEM_H_
#define _SHMEM_H_

typedef struct{
    unsigned char type;
    unsigned char width[2];
    unsigned char height[2];
    unsigned char fps;
    unsigned char flag;
    unsigned char order;
    unsigned char len[4];
    unsigned char data[524276];
}ShmData_Struct;

int shm_create(char *path, int flag, int size, int isService, void **mem);
int shm_destroy(int id);
int process_open(char *cmd);
void process_close(int pid);

#endif
