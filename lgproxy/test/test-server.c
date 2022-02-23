#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>

#include "lgmp/host.h"

void * ram;
#define SHARED_FILE "/dev/shm/lgmp-shared"
#define RAM_SIZE (10*1048576) //10MB

int main(int argc, char * argv[])
{
    int shm = open(SHARED_FILE, O_RDWR | O_CREAT, (mode_t)0600);
    if(shm < 0){
        printf("Unable to open shared memory\n");
        return -1;
    }
    if(ftruncate(shm,RAM_SIZE) != 0){
        printf("Unable to truncate file");
    }
    ram = mmap(0, RAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    if(!ram){
        printf("Memory Mapped failed");
        goto close_fd;
    }

    PLGMPHost host;
    LGMP_STATUS status;

    uint8_t udata[32];
    memset(udata,0xaa, sizeof(udata));

    if((status = lgmpHostInit(ram, RAM_SIZE, &host, sizeof(udata), udata))
        != LGMP_OK)
        {
            printf("Failed Initializing Host: %s\n", lgmpStatusString(status));
            goto unmap;
        }
    
    const struct LGMPQueueConfig conf = {
        .queueID        = 0,
        .numMessages    = 10,
        .subTimeout     = 100
    };

    PLGMPHostQueue queue;
    if((status = lgmpHostQueueNew(host,conf,&queue)) != LGMP_OK)
    {
        printf("Unable to Initialize Host Queue: %s\n", 
            lgmpStatusString(status));
        goto free_host;
    }
    PLGMPMemory mem[10] = { 0 };
    for(int i = 0; i< 10; i++)
    {
        if((status = lgmpHostMemAlloc(host, 1024, &mem[i])) != LGMP_OK)
        {
            printf("LGMP Memory Alloc Failed: %s\n", lgmpStatusString(status));
            goto free_host;
        }
    }
    sprintf(lgmpHostMemPtr(mem[0]), "This is a test from the host application");
    sprintf(lgmpHostMemPtr(mem[1]), "With multiple buffers");
    sprintf(lgmpHostMemPtr(mem[2]), "Containing text");
    sprintf(lgmpHostMemPtr(mem[3]), "That might or might not be");
    sprintf(lgmpHostMemPtr(mem[4]), "interesting.");
    sprintf(lgmpHostMemPtr(mem[5]), "This is buffer number 6");
    sprintf(lgmpHostMemPtr(mem[6]), "Now number 7");
    sprintf(lgmpHostMemPtr(mem[7]), "And now number 8");
    sprintf(lgmpHostMemPtr(mem[8]), "Second last buffer");
    sprintf(lgmpHostMemPtr(mem[9]), "It's over!");

    uint32_t time = 0;
    uint32_t count = 0;
    int pendingAck = 0;

    while(true)
    {
        ++time;

        if((status = lgmpHostQueuePost(queue, count, mem[count % 10])) 
            != LGMP_ERR_QUEUE_FULL) //Check if queue is more than 10
        ++count;

        if (time % 100 == 0)
        {
            if (lgmpHostProcess(host) != LGMP_OK)
            {
                printf("lgmpHostQueuePost Failed: %s\n", lgmpStatusString(status));
                break;
            }

            uint8_t buffer[LGMP_MSGS_SIZE];
            size_t size;
            while((status = lgmpHostReadData(queue, buffer, &size)) == LGMP_OK)
            {
                printf("Read a client message of %d in size\n", size);
                ++pendingAck;
            }

            if (status != LGMP_ERR_QUEUE_EMPTY)
            {
                printf("lgmpHostReadData Failed: %s\n", lgmpStatusString(status));
                break;
            }
            uint32_t newSubs;
            if ((newSubs = lgmpHostQueueNewSubs(queue)) > 0)
                printf("newSubs: %u\n", newSubs);
        }

        if(pendingAck)
        {
        --pendingAck;
        lgmpHostAckData(queue);
        }

        usleep(1000);
    }

    for(int i = 0; i < 10; ++i)
    {
        lgmpHostMemFree(&mem[i]);
    }
    
    return 0;
free_host:
    lgmpHostFree(&host);
unmap:
  munmap(ram, RAM_SIZE);
close_fd:
    close(shm);
    return -1;
}