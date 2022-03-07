#include "common/types.h"
#include "trf_def.h"


static inline uint64_t lpLGToTrfFormat(int lg_type)
{
    switch (lg_type)
    {
    case FRAME_TYPE_BGRA:
        return TRF_TEX_BGRA_8888;
    case FRAME_TYPE_RGBA:
        return TRF_TEX_RGBA_8888;
    case FRAME_TYPE_RGBA16F:
        return TRF_TEX_RGBA_16161616F;
    default:
        return TRF_TEX_INVALID;
    }
}