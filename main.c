#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../common/sample_common.h"
#include "../common/common.h"
#include "rkmedia_api.h"
#include "rkmedia_vdec.h"

#define INBUF_SIZE 4096

#define SCREEN_WIDTH 600
#define SCREEN_HEIGHT 1240

static bool quit = false;

static void sigterm_handler(int sig) {
    fprintf(stderr, "signal %d\n", sig);
    quit = true;
}

#define VDEC_CHN_NUM 0
#define VI_VO_CHN_NUM 1

static void *decode_media_buffer(void *arg) {
    (void) arg;
    MEDIA_BUFFER mb = NULL;
    int ret = 0;

    MPP_CHN_S VdecChn, VoChn;
    VdecChn.enModId = RK_ID_VDEC;
    VdecChn.s32DevId = 0;
    VdecChn.s32ChnId = 0;
    VoChn.enModId = RK_ID_VO;
    VoChn.s32DevId = 0;
    VoChn.s32ChnId = VDEC_CHN_NUM;

    mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VDEC, VDEC_CHN_NUM, 5000);
    if (!mb) {
        printf("RK_MPI_SYS_GetMediaBuffer get null buffer in 5s...\n");
        return NULL;
    }

    MB_IMAGE_INFO_S stImageInfo = {0};
    ret = RK_MPI_MB_GetImageInfo(mb, &stImageInfo);
    if (ret) {
        printf("Get image info failed! ret = %d\n", ret);
        RK_MPI_MB_ReleaseBuffer(mb);
        return NULL;
    }

    printf("Get Frame:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, "
           "timestamp:%lld, ImgInfo:<wxh %dx%d, fmt 0x%x>\n",
           RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb),
           RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetChannelID(mb),
           RK_MPI_MB_GetTimestamp(mb), stImageInfo.u32Width,
           stImageInfo.u32Height, stImageInfo.enImgType);
    RK_MPI_MB_ReleaseBuffer(mb);

    VO_CHN_ATTR_S stVoAttr = {0};
    memset(&stVoAttr, 0, sizeof(stVoAttr));
    stVoAttr.pcDevNode = "/dev/dri/card0";
    stVoAttr.emPlaneType = VO_PLANE_OVERLAY; // VO_PLANE_OVERLAY; // VO_PLANE_OVERLAY;  NV12
    stVoAttr.enImgType = stImageInfo.enImgType;
    stVoAttr.u16Zpos = 1;
    // stVoAttr.u32Width = SCREEN_WIDTH;
    // stVoAttr.u32Height = SCREEN_HEIGHT;
    stVoAttr.stImgRect.s32X = 0;
    stVoAttr.stImgRect.s32Y = 0;
    stVoAttr.stImgRect.u32Width = stImageInfo.u32Width;
    stVoAttr.stImgRect.u32Height = stImageInfo.u32Height;
    stVoAttr.stDispRect.s32X = 540;
    stVoAttr.stDispRect.s32Y = 300 - stImageInfo.u32Height / 4;
    stVoAttr.stDispRect.u32Width = stImageInfo.u32Width / 2;
    stVoAttr.stDispRect.u32Height = stImageInfo.u32Height / 2;

    printf("video width pos :%d \n", stImageInfo.u32Width / 2); // 320

    ret = RK_MPI_VO_CreateChn(VDEC_CHN_NUM, &stVoAttr);
    if (ret) {
        printf("Create VO[0] failed! ret=%d\n", ret);
        quit = false;
        return NULL;
    }

    // VDEC->VO
    ret = RK_MPI_SYS_Bind(&VdecChn, &VoChn);
    if (ret) {
        printf("Bind VDEC[0] to VO[1] failed! ret=%d\n", ret);
        quit = false;
        return NULL;
    }

    while (!quit) {
        usleep(500000);
    }

    ret = RK_MPI_SYS_UnBind(&VdecChn, &VoChn);
    if (ret)
        printf("UnBind VDECp[0] to VO[0] failed! ret=%d\n", ret);

    ret = RK_MPI_VO_DestroyChn(VoChn.s32ChnId);
    if (ret)
        printf("Destroy VO[0] failed! ret=%d\n", ret);

    return NULL;
}

static RK_CHAR optstr[] = "?::i:o:f:w:h:t:l:";

static void print_usage() {
    printf("usage example: rkmedia_vdec_test -w 720 -h 480 -i /userdata/out.jpeg "
           "-f 0 -t JPEG.\n");
    printf("\t-w: DisplayWidth, Default: 720\n");
    printf("\t-h: DisplayHeight, Default: 1280\n");
    printf("\t-i: InputFilePath, Default: NULL\n");
    printf("\t-f: 1:hardware; 0:software. Default:hardware\n");
    printf("\t-l: LoopSwitch; 0:NoLoop; 1:Loop. Default: 0.\n");
    printf("\t-t: codec type, Default H264, support H264/H265/JPEG.\n");
}

int vi_vo(void) {
    int ret = 0;
    int video_width = 800;
    int video_height = 600;
    int disp_width = 1024; // 720;
    int disp_height = 600; // 1280;
    RK_S32 s32CamId = 0;
    int fps = 30;

    SAMPLE_COMM_ISP_Init(s32CamId, RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, "/oem/etc/iqfiles");
    SAMPLE_COMM_ISP_Run(s32CamId);
    SAMPLE_COMM_ISP_SetFrameRate(s32CamId, fps);

    RK_MPI_SYS_Init();
    VI_CHN_ATTR_S vi_chn_attr;
    vi_chn_attr.pcVideoNode = "rkispp_scale0";
    vi_chn_attr.u32BufCnt = 1;
    vi_chn_attr.u32Width = video_width;
    vi_chn_attr.u32Height = video_height;
    vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
    vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
    ret = RK_MPI_VI_SetChnAttr(s32CamId, 0, &vi_chn_attr);
    ret |= RK_MPI_VI_EnableChn(s32CamId, 0);
    if (ret) {
        printf("Create vi[0] failed! ret=%d\n", ret);
        return -1;
    }

    // rga0 for primary plane
    RGA_ATTR_S stRgaAttr;
    memset(&stRgaAttr, 0, sizeof(stRgaAttr));
    stRgaAttr.bEnBufPool = RK_TRUE;
    stRgaAttr.u16BufPoolCnt = 3;
    stRgaAttr.u16Rotaion = 0;
    stRgaAttr.stImgIn.u32X = 0;
    stRgaAttr.stImgIn.u32Y = 0;
    stRgaAttr.stImgIn.imgType = IMAGE_TYPE_NV12;
    stRgaAttr.stImgIn.u32Width = video_width;
    stRgaAttr.stImgIn.u32Height = video_height;
    stRgaAttr.stImgIn.u32HorStride = video_width;
    stRgaAttr.stImgIn.u32VirStride = video_height;

    stRgaAttr.stImgOut.u32X = 0;
    stRgaAttr.stImgOut.u32Y = 0;
    stRgaAttr.stImgOut.imgType = IMAGE_TYPE_RGB888;
    stRgaAttr.stImgOut.u32Width = disp_width;
    stRgaAttr.stImgOut.u32Height = disp_height;
    stRgaAttr.stImgOut.u32HorStride = disp_width;
    stRgaAttr.stImgOut.u32VirStride = disp_height;
    ret = RK_MPI_RGA_CreateChn(0, &stRgaAttr);
    if (ret) {
        printf("Create rga[0] falied! ret=%d\n", ret);
        return -1;
    }

    VO_CHN_ATTR_S stVoAttr = {0};
    // VO[0] for primary plane
    stVoAttr.pcDevNode = "/dev/dri/card0";
    stVoAttr.emPlaneType = VO_PLANE_PRIMARY; // VO_PLANE_PRIMARY;
    stVoAttr.enImgType = IMAGE_TYPE_RGB888;
    stVoAttr.u16Zpos = 0;
    stVoAttr.stDispRect.s32X = 0; // 0;
    stVoAttr.stDispRect.s32Y = 0;
    stVoAttr.stDispRect.u32Width = 1024; // (1024 - 320); // disp_width;
    stVoAttr.stDispRect.u32Height = 600; // disp_height;

    ret = RK_MPI_VO_CreateChn(VI_VO_CHN_NUM, &stVoAttr);
    if (ret) {
        printf("Create vo[0] failed! ret=%d\n", ret);
        return -1;
    }

    MPP_CHN_S stSrcChn = {0};
    MPP_CHN_S stDestChn = {0};

    printf("#Bind VI[0] to RGA[0]....\n");
    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32ChnId = 0;
    stDestChn.enModId = RK_ID_RGA;
    stDestChn.s32ChnId = 0;
    ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (ret) {
        printf("Bind vi[0] to rga[0] failed! ret=%d\n", ret);
        return -1;
    }

    printf("# Bind RGA[0] to VO[0]....\n");
    stSrcChn.enModId = RK_ID_RGA;
    stSrcChn.s32ChnId = 0;
    stDestChn.enModId = RK_ID_VO;
    stDestChn.s32ChnId = VI_VO_CHN_NUM;
    ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (ret) {
        printf("Bind rga[0] to vo[1] failed! ret=%d\n", ret);
        return -1;
    }

    printf("%s initial finish\n", __func__);

    return 0;
}

int main__(void) {
    vi_vo();
    while (1) {
        usleep(1000);
    }
}

MPP_CHN_S stViChn;
MPP_CHN_S stRgaChn;
MPP_CHN_S stVoChn;
static RK_S32 s32CamId = 0;
pthread_t vi_vo_buffer_thread_id;

static void *vi_vo_buffer_thread(void *arg) {

    int logCounter = 0;
    MEDIA_BUFFER buffer;

    while (quit == true) {
        buffer = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, stViChn.s32ChnId, -1);
        if (!buffer) {
            printf("get vi buffer failed\n");
            break;
        }
        logCounter++;
        if (logCounter > 100) {
            printf("normally output every 2 seconds\n");
            print_mb_info(buffer);
            logCounter = 0;
        }

        RK_MPI_SYS_SendMediaBuffer(RK_ID_VO, stVoChn.s32ChnId, buffer);
        RK_MPI_MB_ReleaseBuffer(buffer);
        usleep(1000);
    }
}

int vi_vo_direct(void) {
    int ret = 0;
    memset(&stVoChn, 0, sizeof(stVoChn));
    memset(&stViChn, 0, sizeof(stViChn));

    s32CamId = 0;
    int logCounter = 0;

    ret = init_isp(s32CamId);
    if (ret != 0) {
        printf("init_isp failed");
        return -1;
    }

    stViChn.s32DevId = s32CamId;
    stViChn.enModId = RK_ID_VI;
    stViChn.s32ChnId = 1;

    stVoChn.enModId = RK_ID_VO;
    stVoChn.s32ChnId = VI_VO_CHN_NUM;

    ret = init_vi(1024, 600, s32CamId, stViChn.s32ChnId);
    // ret = init_vi(1920, 1080, s32CamId, stViChn.s32ChnId);
    if (ret) {
        printf("init vi failed\n");
        return -1;
    }

    // 1920/1080 = 1.777
    // 1024/600 = 1.706 almost same
    ret = init_vo_primary(0, 0, 533, 600, 0, 0, 533, 600, stVoChn.s32ChnId);
    // ret = init_vo_overlay(0, 0, 533, 600, 0, 0, 533, 600, stVoChn.s32ChnId);
    // ret = init_vo_overlay(704, (1080 - 600) / 2, 533, 600, 0, 0, 533, 600, stVoChn.s32ChnId);
    // ret = init_vo_overlay((1920 - 533) / 2, 0, (1920 - 533) / 2, 600, 0, 0, 533, 600, stVoChn.s32ChnId);
    if (ret) {
        printf("init vo failed\n");
        return -1;
    }

    ret = bind_vi_vo(stViChn, stVoChn);
    if (ret) {
        printf("bind vi and vo failed\n");
        return -1;
    }

    // pthread_create(&vi_vo_buffer_thread_id, NULL, vi_vo_buffer_thread, NULL);
}

int main(int argc, char *argv[]) {
    RK_CHAR *pcFileName = NULL;
    RK_U32 u32DispWidth = 1024;
    RK_U32 u32DispHeight = 600;
    RK_BOOL bIsHardware = RK_TRUE;
    RK_U32 u32Loop = 1;
    CODEC_TYPE_E enCodecType = RK_CODEC_TYPE_H264;
    int c, ret;

    printf("start \n");

    while ((c = getopt(argc, argv, optstr)) != -1) {
        switch (c) {
            case 'w':
                u32DispWidth = atoi(optarg);
                break;
            case 'h':
                u32DispHeight = atoi(optarg);
                break;
            case 'i':
                pcFileName = optarg;
                break;
            case 'f':
                bIsHardware = atoi(optarg) ? RK_TRUE : RK_FALSE;
                break;
            case 'l':
                u32Loop = atoi(optarg);
                break;
            case 't':
                if (strcmp(optarg, "H264") == 0) {
                    enCodecType = RK_CODEC_TYPE_H264;
                } else if (strcmp(optarg, "H265") == 0) {
                    enCodecType = RK_CODEC_TYPE_H265;
                } else if (strcmp(optarg, "JPEG") == 0) {
                    enCodecType = RK_CODEC_TYPE_JPEG;
                }
                break;
            case '?':
            default:
                print_usage();
                return 0;
        }
    }

    // primary plane fmt: RGB888
    // overlay plane fmt: NV12

    printf("#FileName: %s\n", pcFileName);
    printf("#Display wxh: %dx%d\n", u32DispWidth, u32DispHeight);
    printf("#Decode Mode: %s\n", bIsHardware ? "Hardware" : "Software");
    printf("#Loop Cnt: %d\n", u32Loop);

    signal(SIGINT, sigterm_handler);

    ret = vi_vo_test();
    // ret = vi_vo();
    // ret = vi_vo_direct();
    if (ret < 0) {
        printf("vi vo init failed\n");
        return -1;
    }

    FILE *infile = fopen(pcFileName, "rb");
    if (!infile) {
        fprintf(stderr, "Could not open %s\n", pcFileName);
        return 0;
    }

    // RK_MPI_SYS_Init();
    // VDEC
    VDEC_CHN_ATTR_S stVdecAttr;
    stVdecAttr.enCodecType = enCodecType;
    stVdecAttr.enMode = VIDEO_MODE_FRAME;
    if (bIsHardware) {
        if (stVdecAttr.enCodecType == RK_CODEC_TYPE_JPEG) {
            stVdecAttr.enMode = VIDEO_MODE_FRAME;
        } else {
            stVdecAttr.enMode = VIDEO_MODE_STREAM;
        }
        stVdecAttr.enDecodecMode = VIDEO_DECODEC_HADRWARE;
    } else {
        stVdecAttr.enMode = VIDEO_MODE_FRAME;
        stVdecAttr.enDecodecMode = VIDEO_DECODEC_SOFTWARE;
    }

    ret = RK_MPI_VDEC_CreateChn(0, &stVdecAttr);
    if (ret) {
        printf("Create Vdec[0] failed! ret=%d\n", ret);
        return -1;
    }

    pthread_t decode_media_thread_id;
    pthread_create(&decode_media_thread_id, NULL, decode_media_buffer, NULL);

    int data_size;
    int read_size;
    if (stVdecAttr.enMode == VIDEO_MODE_STREAM) {
        data_size = INBUF_SIZE;
    } else if (stVdecAttr.enMode == VIDEO_MODE_FRAME) {
        fseek(infile, 0, SEEK_END);
        data_size = ftell(infile);
        fseek(infile, 0, SEEK_SET);
    }

    while (!quit) {
        MEDIA_BUFFER mb = RK_MPI_MB_CreateBuffer(data_size, RK_FALSE, 0);
        RETRY:
        /* read raw data from the input file */
        read_size = fread(RK_MPI_MB_GetPtr(mb), 1, data_size, infile);
        if (!read_size || feof(infile)) {
            if (u32Loop) {
                fseek(infile, 0, SEEK_SET);
                goto RETRY;
            } else {
                RK_MPI_MB_ReleaseBuffer(mb);
                break;
            }
        }
        RK_MPI_MB_SetSize(mb, read_size);
        printf("#Send packet(%p, %zuBytes) to VDEC[1].\n", RK_MPI_MB_GetPtr(mb),
               RK_MPI_MB_GetSize(mb));
        ret = RK_MPI_SYS_SendMediaBuffer(RK_ID_VDEC, VDEC_CHN_NUM, mb);
        RK_MPI_MB_ReleaseBuffer(mb);

        usleep(30 * 1000);
    }

    quit = true;
    pthread_join(decode_media_thread_id, NULL);

    RK_MPI_VDEC_DestroyChn(0);
    fclose(infile);

    return 0;
}
