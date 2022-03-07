#include"host.h"

int main(int argc, char ** argv)
{
    char* host = "0.0.0.0";
    char* port = "35000";

    PTRFContext ctx = trfAllocContext(); //Initialize Server Context
    PTRFContext client_ctx;
    
    if(lpInitServer(ctx, client_ctx, host, port) < 0)
    {
        trfDestroyContext(client_ctx);
        trfDestroyContext(ctx);
        printf("Unable to initiate client\n");
    }

    trfDestroyContext(client_ctx);
    trfDestroyContext(ctx);
}

int lpInitServer(PTRFContext ctx, PTRFContext client_ctx, 
            char* host, char* port)
{
    int ret = 0;

    if ((ret = trfNCServerInit(ctx, host, port)) < 0)
    {
        printf("Unable to initiate server\n");
        return ret;
    }
    if ((ret = trfNCAccept(ctx, &client_ctx)) < 0)
    {
        printf("Unable to accept client\n");
        return ret;
    }
    return 0;
}