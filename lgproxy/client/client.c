#include "client.h"

int main(int argc, char** argv)
{
    char* host = "127.0.0.1";
    char* port = "35000";

    PTRFContext ctx = trfAllocContext();
    if (trfNCClientInit(ctx,host,port) < 0)
    {
        printf("unable to initiate client\n");
        fflush(stdout);
        return -1;
    }
    printf("Hello\n");
}