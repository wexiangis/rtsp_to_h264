#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#include "shmem.h"

int shm_create(char *path, int flag, int size, void **mem)
{
    key_t key = ftok(path, flag);
    if(key < 0)
    {
        fprintf(stderr, "get key error\n");
        return -1;
    }

    int id;
    id = shmget(key, size, 0666);
    if(id < 0)
        id = shmget(key, size, IPC_CREAT|0666);

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

pid_t process_rtspToH264(char *filePath, char *url)
{
    // pid_t pid;
    // if(!filePath || !url)
    //     return 0;
    // pid = fork();
    // if(pid < 0)
    //     return 0;
    // else if(pid == 0) //child process
    // {
    //     execl(filePath, "rtspToH264", url, 
    //         "-shm", (char *)0);
    //     _exit(127);
    // }
    // return pid;

    pid_t pid = 10086;
    char cmd[128] = {0};
    snprintf(cmd, 128, "%s %s -shm &", filePath, url);
    system(cmd);
    return pid;
}

void process_rtspToH264_close(pid_t pid)
{
    if(pid)
        system("killall -10 rtspToH264");
}

pid_t process_open(char *cmd)
{
    pid_t pid;
    if(!cmd)
        return 0;
    if((pid = fork()) < 0)
        return 0;
    else if(pid == 0) //child process
    {
        execl("/bin/sh", "sh", "-c", cmd, (char *)0);//最后那个0是用来告诉系统传参结束的
        _exit(127);
    }
    return pid;
}

void process_close(pid_t *pid)
{
    if(pid)
    {
        if(waitpid(*pid, NULL, WNOHANG|WUNTRACED) != *pid)
            kill(*pid, SIGUSR1);
        *pid = 0;
    }
}



