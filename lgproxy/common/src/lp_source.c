#include "lp_source.h"

// static struct LPProxyClient {
//     PLPContext      lp_ctx;
// };

// void * proxyClient(struct LPProxyClient);

int main(int argc, char ** argv)
{
    // PTRFContext ctx = trfAllocContext();
    PLPContext ctx = lpAllocContext();
    if (!ctx)
        return ENOMEM;

    char * host = NULL;
    char * port = NULL;

    int o;
    while ((o = getopt(argc, argv, "h:p:")) != -1)
    {
        switch (o)
        {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case '?':
                lp__log_fatal("Invalid argument -%c", optopt);
                return EINVAL;
        }
    }

    if (!host || !port)
    {
        lp__log_fatal("No host or port specified");
        return EINVAL;
    }

    int ret;

    if((ret = lpTrfServerInit(ctx, host, port)) < 0)
    {
        lp__log_fatal("Unable to initialize libtrf server");
        return -1;
    }

    lpInitLgmpClient()

    // ret = trfNCServerInit(ctx, host, port);
    // if (ret < 0)
    // {
    //     lp__log_fatal("Server initialization failed");
    //     return -ret;
    // }

    // PTRFContext cli_ctx = NULL;
    // ret = trfNCAccept(ctx, cli_ctx);
    // if (ret < 0 || !cli_ctx)
    // {
    //     lp__log_fatal("Client accept failed");
    //     trfDestroyContext(ctx);
    //     return -ret;
    // }

    // struct LPProxyClient cli = {
    //     .lp_ctx = lpAllocContext()
    // };

    // if (!cli.lp_ctx)
    // {
    //     trfDestroyContext(ctx);
    //     return ENOMEM;
    // }

}