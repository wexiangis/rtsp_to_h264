
#ifndef _SHMEM_H_
#define _SHMEM_H_

#include <sys/types.h>

typedef struct{
    unsigned char type;
    unsigned char width[2];
    unsigned char height[2];
    unsigned char fps;
    unsigned char ready;
    unsigned char order;
    unsigned char len[4];
    unsigned char data[524276];
}ShmData_Struct;

int shm_create(char *path, int flag, int size, int isService, void **mem);
int shm_destroy(int id);

pid_t process_rtspToH264(char *filePath, char *url);
pid_t process_open(char *cmd);
void process_close(pid_t *pid);

#endif
