#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include "lgmp/client.h"

void * ram;
#define SHARED_FILE "/dev/shm/lgmp-shared"
#define RAM_SIZE (10*1048576)

int main(int argc, char * argv[])
{
    int delay = 50;
    int ret = 0;
    int shm = open(SHARED_FILE, O_RDWR, (mode_t)0600);
    if (shm < 0)
    {
        printf("Unable to open shared memory\n");
        return -1;
    }
    ram = mmap(0, RAM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shm, 0);
    if(!ram){
        printf("Failed to map memory");
        ret = -ENOMEM;
        goto close_fd;
    }
    PLGMPClient client;
    LGMP_STATUS status;

    do { // Initate Client //!Add Timeout
        status = lgmpClientInit(ram, RAM_SIZE, &client);
        if (status != LGMP_OK){
            printf("Unable to Initiate Client %s\n", lgmpStatusString(status));
        } 
    } while (status != LGMP_OK);

    uint32_t udataSize;
    uint8_t *udata;

    do { // Init Session //!Add Timeout 
        status = lgmpClientSessionInit(client, &udataSize, &udata);
        if (status != LGMP_OK){
            usleep(100000);
            printf("Unable to Init Session: %s\n", lgmpStatusString(status));
        }
    } while(status != LGMP_OK);
    printf("Session Valid\n");

    PLGMPClientQueue queue;
    do{
        status = lgmpClientSubscribe(client, 0, &queue);
        if (status == LGMP_ERR_NO_SUCH_QUEUE){
            usleep(250000);
        }
        else if (status != LGMP_OK){
            printf("Unable Subscribe: %s\n", lgmpStatusString(status));
            ret = -status;
            goto free_client;
        }
    } while(status != LGMP_OK);

    uint32_t serial;
    bool done = false;

    uint8_t data[32];
    for (int i = 0; i < 20; i++){
        if((status = lgmpClientSendData(queue, data, sizeof(data), &serial))
            != LGMP_OK){
                if(status == LGMP_ERR_QUEUE_FULL){
                    i--;
                    continue;
                }
                printf("lgmpClientSendData: %s\n", lgmpStatusString(status));
                ret = -status;
                goto free_client;    
        }
    }
    uint32_t lastCount = 0;
    while(lgmpClientSessionValid(client))
    {
        LGMPMessage msg;
        if((status = lgmpClientProcess(queue, &msg)) != LGMP_OK)
        {
            if(status == LGMP_ERR_QUEUE_EMPTY){
                usleep(1);
                continue;
            }
            else{
                printf("lgmpClientProcess: %s\n", lgmpStatusString(status));
                ret = -status;
                goto unsubscribe;
            }
        }
        if(delay){
            printf("Received %4u: %s\n", msg.udata, (char*)msg.mem);
        }
        if(!lastCount){
            lastCount = msg.udata;
        }
        else{
            if(lastCount != msg.udata - 1){
                printf("Missed Message\n");
                ret = -1;
                goto unsubscribe;
            }
            lastCount = msg.udata;
        }
        if(delay){
            usleep(50);
        }
        status = lgmpClientMessageDone(queue);
        if(status != LGMP_OK){
            printf("lgmpClientMessageDone: %s\n", lgmpStatusString(status));
        }
        if(!done){
            uint32_t hostSerial;
            lgmpClientGetSerial(queue,&hostSerial);
            printf("Serial: %d --> hostSerial %d\n", serial, hostSerial);
            if(hostSerial >= serial){
                done = true;
                printf("All Data Received\n");
                goto unsubscribe;
            }
        }
    }

unsubscribe:
    if(lgmpClientSessionValid(client)){
        lgmpClientUnsubscribe(&queue);
    }
free_client:
    lgmpClientFree(&client);
unmap:
    munmap(ram, RAM_SIZE);
close_fd:
    close(shm);
    return ret;
}