#include "lp_sink.h"


int main(int argc, char ** argv){
    PLPContext ctx = lpAllocContext();
    if (!ctx) {
        return - EINVAL;
    }
    int ret = 0;
    char * host = NULL;
    char * port = NULL;

    int o;
    while ((o = getopt(argc, argv, "h:p:f:s:")) != -1)
    {
        switch (o)
        {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'f':
                ctx->shm = optarg;
            case 's':
                ctx->ram_size = (uint32_t) atoi(optarg);
            case '?':
                lp__log_fatal("Invalid argument -%c", optopt);
                return EINVAL;
        }
    }

    if ((ret = lpTrfClientInit(ctx, host, port)) < 0)
    {
        lp__log_error("Unable to initialize trf client");
        return -1;
    }

    PTRFDisplay displays;
    printf("Retrieving displays");
    ret = trfGetServerDisplays(ctx->client_ctx, &displays);
    if (ret < 0)
    {
        lp__log_error("Unable to get servers display list");
        return -1;
    }

    printf("Server Display List\n");
    printf("---------------------------------------------\n");
    for (PTRFDisplay tmp = displays; tmp != NULL; tmp = tmp->next)
    {
        printf("Display ID:    %d\n", tmp->id);
        printf("Display Name:  %s\n", tmp->name);
        printf("Resolution:    %d x %d\n", tmp->width, tmp->height);
        printf("Refresh Rate:  %d\n", tmp->rate);
        printf("Pixel Format:  %d\n", tmp->format);
        printf("Display Group: %d\n", tmp->dgid);
        printf("Group Offset:  %d, %d\n", tmp->x_offset, tmp->y_offset);
        printf("---------------------------------------------\n");
    }
}