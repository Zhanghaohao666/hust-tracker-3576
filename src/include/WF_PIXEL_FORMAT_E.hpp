// 
#define WF_VIDEO_FMT_MASK                 0x000f0000
#define WF_VIDEO_FMT_YUV                     0x00000000
#define WF_VIDEO_FMT_RGB                    0x00010000
#define WF_VIDEO_FMT_BAYER                0X00020000

typedef enum wf_PIXEL_FORMAT_E {
    WF_FMT_UNDEFINED=-1,
    WF_FMT_YUV420SP         = WF_VIDEO_FMT_YUV,        /* YYYY... UV...            */
    WF_FMT_YUV420SP_10BIT,
    WF_FMT_YUV422SP,                                   /* YYYY... UVUV...          */
    WF_FMT_YUV422SP_10BIT,                             ///< Not part of ABI
    WF_FMT_YUV420P,                                    /* YYYY... UUUU... VVVV     */
    WF_FMT_YUV420P_VU,                                 /* YYYY... VVVV... UUUU     */
    WF_FMT_YUV420SP_VU,                                /* YYYY... VUVUVU...        */
    WF_FMT_YUV422P,                                    /* YYYY... UUUU... VVVV     */
    WF_FMT_YUV422SP_VU,                                /* YYYY... VUVUVU...        */
    WF_FMT_YUV422_YUYV,                                /* YUYVYUYV...              */
    WF_FMT_YUV422_UYVY,                                /* UYVYUYVY...              */
    WF_FMT_YUV400SP,                                   /* YYYY...                  */
    WF_FMT_YUV440SP,                                   /* YYYY... UVUV...          */
    WF_FMT_YUV411SP,                                   /* YYYY... UV...            */
    WF_FMT_YUV444,                                     /* YUVYUVYUV...             */
    WF_FMT_YUV444SP,                                   /* YYYY... UVUVUVUV...      */
    WF_FMT_YUV422_YVYU,                                /* YVYUYVYU...              */
    WF_FMT_YUV422_VYUY,                                /* VYUYVYUY...              */
    WF_FMT_YUV_BUTT,

    WF_FMT_RGB565          = WF_VIDEO_FMT_RGB,         /* 16-bit RGB               */
    WF_FMT_BGR565,                                     /* 16-bit RGB               */
    WF_FMT_RGB555,                                     /* 15-bit RGB               */
    WF_FMT_BGR555,                                     /* 15-bit RGB               */
    WF_FMT_RGB444,                                     /* 12-bit RGB               */
    WF_FMT_BGR444,                                     /* 12-bit RGB               */
    WF_FMT_RGB888,                                     /* 24-bit RGB               */
    WF_FMT_BGR888,                                     /* 24-bit RGB               */
    WF_FMT_RGB101010,                                  /* 30-bit RGB               */
    WF_FMT_BGR101010,                                  /* 30-bit RGB               */
    WF_FMT_ARGB1555,                                   /* 16-bit RGB               */
    WF_FMT_ABGR1555,                                   /* 16-bit RGB               */
    WF_FMT_ARGB4444,                                   /* 16-bit RGB               */
    WF_FMT_ABGR4444,                                   /* 16-bit RGB               */
    WF_FMT_ARGB8565,                                   /* 24-bit RGB               */
    WF_FMT_ABGR8565,                                   /* 24-bit RGB               */
    WF_FMT_ARGB8888,                                   /* 32-bit RGB               */
    WF_FMT_ABGR8888,                                   /* 32-bit RGB               */
    WF_FMT_BGRA8888,                                   /* 32-bit RGB               */
    WF_FMT_RGBA8888,                                   /* 32-bit RGB               */
    WF_FMT_RGBA5551,                                   /* 16-bit RGB               */
    WF_FMT_BGRA5551,                                   /* 16-bit RGB               */
    WF_FMT_BGRA4444,                                   /* 16-bit RGB               */
    WF_FMT_RGB_BUTT,

    WF_FMT_RGB_BAYER_SBGGR_8BPP = WF_VIDEO_FMT_BAYER,  /* 8-bit raw                */
    WF_FMT_RGB_BAYER_SGBRG_8BPP,                       /* 8-bit raw                */
    WF_FMT_RGB_BAYER_SGRBG_8BPP,                       /* 8-bit raw                */
    WF_FMT_RGB_BAYER_SRGGB_8BPP,                       /* 8-bit raw                */
    WF_FMT_RGB_BAYER_SBGGR_10BPP,                      /* 10-bit raw               */
    WF_FMT_RGB_BAYER_SGBRG_10BPP,                      /* 10-bit raw               */
    WF_FMT_RGB_BAYER_SGRBG_10BPP,                      /* 10-bit raw               */
    WF_FMT_RGB_BAYER_SRGGB_10BPP,                      /* 10-bit raw               */
    WF_FMT_RGB_BAYER_SBGGR_12BPP,                      /* 12-bit raw               */
    WF_FMT_RGB_BAYER_SGBRG_12BPP,                      /* 12-bit raw               */
    WF_FMT_RGB_BAYER_SGRBG_12BPP,                      /* 12-bit raw               */
    WF_FMT_RGB_BAYER_SRGGB_12BPP,                      /* 12-bit raw               */
    WF_FMT_RGB_BAYER_14BPP,                            /* 14-bit raw               */
    WF_FMT_RGB_BAYER_SBGGR_16BPP,                      /* 16-bit raw               */
    WF_FMT_RGB_BAYER_BUTT,
    WF_FMT_BUTT            = WF_FMT_RGB_BAYER_BUTT,
    WF_FMT_GRAY_U16C1,
    WF_FMT_GRAY_U8C1,
} WF_PIXEL_FORMAT_E;
