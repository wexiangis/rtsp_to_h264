#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/shm.h>

int shm_create(char *path, int flag, int size, int isService, void **mem)
{
    key_t key = ftok(path, flag);
    if(key < 0)
    {
        fprintf(stderr, "get key error\n");
        return -1;
    }

    int id;
    if(isService)
        id = shmget(key, size, IPC_CREAT|0666);
    else
        id = shmget(key, size, 0666);
    if(id < 0)
    {
        fprintf(stderr, "get id error\n");
        return -1;
    }

    if(mem)
        *mem = shmat(id, NULL, 0);

    return id;
}

int shm_destroy(int id)
{
	return shmctl(id,IPC_RMID,NULL);
}


