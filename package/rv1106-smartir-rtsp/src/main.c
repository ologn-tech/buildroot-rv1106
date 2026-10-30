#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/v4l2-controls.h>
#include <linux/v4l2-subdev.h>

#include <rk_aiq_user_api2_sysctl.h>
#include <rk_smart_ir_api.h>
#include <rk_mpi_mb.h>
#include <rk_mpi_sys.h>
#include <rk_mpi_venc.h>
#include <rk_mpi_vi.h>
#include <rtsp_demo.h>
#include <im2d.h>
#include <rga.h>

#include "font8x8.h"

#ifndef WIDTH
#define DISP_WIDTH 1920
#else
#define DISP_WIDTH WIDTH
#endif

#ifndef HEIGHT
#define DISP_HEIGHT 1080
#else
#define DISP_HEIGHT HEIGHT
#endif

#define RK_ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

#ifndef V4L2_CID_IMAGE_SOURCE_CLASS_BASE
#define V4L2_CID_IMAGE_SOURCE_CLASS_BASE (0x009e0900)
#endif

#ifndef V4L2_CID_ANALOGUE_GAIN
#define V4L2_CID_ANALOGUE_GAIN (V4L2_CID_IMAGE_SOURCE_CLASS_BASE + 3)
#endif

#define V4L2_SUBDEV_MAX 32

static const unsigned char *font_glyph(char c)
{
	unsigned char idx = (unsigned char)c;

	if (idx >= 128) {
		return font8x8_basic[' '];
	}
	return font8x8_basic[idx];
}

static void draw_string(unsigned char *img, int stride, int width, int height,
			int x, int y, const char *text, int scale,
			unsigned char b, unsigned char g, unsigned char r)
{
	int cursor = x;

	if (scale < 1) {
		scale = 1;
	}

	for (const char *c = text; *c != '\0'; c++) {
		const unsigned char *glyph = font_glyph(*c);

		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				if (!(glyph[row] & (0x01 << col))) {
					continue;
				}
				for (int sy = 0; sy < scale; sy++) {
					int py = y + row * scale + sy;

					if (py < 0 || py >= height) {
						continue;
					}
					for (int sx = 0; sx < scale; sx++) {
						int px = cursor + col * scale +
							 sx;

						if (px < 0 || px >= width) {
							continue;
						}
						unsigned char *p =
							img +
							((size_t)py * stride +
							 px) * 3;

						p[0] = b;
						p[1] = g;
						p[2] = r;
					}
				}
			}
		}

		cursor += 8 * scale;
	}
}

static RK_U64 get_now_us(void)
{
	struct timespec time = {0, 0};

	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000;
}

static int v4l2_subdev_has_ae_controls(int fd)
{
	struct v4l2_queryctrl qctrl;

	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = V4L2_CID_EXPOSURE;
	if (ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) != 0 ||
	    (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
		return 0;
	}

	qctrl.id = V4L2_CID_ANALOGUE_GAIN;
	if (ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) != 0 ||
	    (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
		return 0;
	}

	return 1;
}

static int v4l2_open_sensor_subdev(const char *subdev_path)
{
	char path[64];
	int fd;

	if (subdev_path) {
		fd = open(subdev_path, O_RDWR | O_CLOEXEC);
		if (fd < 0) {
			fprintf(stderr, "Failed to open %s: %s\n", subdev_path,
				strerror(errno));
			return -1;
		}
		if (!v4l2_subdev_has_ae_controls(fd)) {
			fprintf(stderr,
				"%s has no exposure/analogue_gain controls\n",
				subdev_path);
			close(fd);
			return -1;
		}
		return fd;
	}

	for (int i = 0; i < V4L2_SUBDEV_MAX; i++) {
		snprintf(path, sizeof(path), "/dev/v4l-subdev%d", i);
		fd = open(path, O_RDWR | O_CLOEXEC);
		if (fd < 0) {
			continue;
		}
		if (v4l2_subdev_has_ae_controls(fd)) {
			return fd;
		}
		close(fd);
	}

	fprintf(stderr,
		"Failed to find sensor subdev with exposure/analogue_gain\n");
	return -1;
}

static int v4l2_read_ae(int fd, int *exposure, int *analogue_gain)
{
	struct v4l2_control ctrl;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_EXPOSURE;
	if (ioctl(fd, VIDIOC_G_CTRL, &ctrl) != 0) {
		return -1;
	}
	*exposure = ctrl.value;

	ctrl.id = V4L2_CID_ANALOGUE_GAIN;
	if (ioctl(fd, VIDIOC_G_CTRL, &ctrl) != 0) {
		return -1;
	}
	*analogue_gain = ctrl.value;

	return 0;
}

static rk_aiq_sys_ctx_t *aiq_ctx;
static rk_aiq_static_info_t aiq_static_info;
static rk_smart_ir_ctx_t *smartIr_ctx;
static const char *scene_mode = "day";
static pthread_t smartIr_tid;
static volatile int smartIr_quit;
static volatile int smartIr_started;
static RK_SMART_IR_STATUS_t smartIr_last_status =
	RK_SMART_IR_STATUS_INVALID;

static void smartIr_apply_status(RK_SMART_IR_STATUS_t status)
{
	const char *next_mode;

	if (status == RK_SMART_IR_STATUS_NIGHT) {
		next_mode = "night";
	} else if (status == RK_SMART_IR_STATUS_DAY) {
		next_mode = "day";
	} else {
		return;
	}

	if (status == smartIr_last_status) {
		return;
	}

	if (rk_aiq_uapi2_sysctl_switch_scene(aiq_ctx, "normal", next_mode) ==
	    XCAM_RETURN_NO_ERROR) {
		scene_mode = next_mode;
		smartIr_last_status = status;
		printf("SmartIR: switched to %s\n", next_mode);
	} else {
		fprintf(stderr, "SmartIR: switch_scene(%s) failed\n",
			next_mode);
	}
}

static void *smartIr_thread(void *arg)
{
	(void)arg;

	while (!smartIr_quit) {
		rk_aiq_isp_stats_t *stats_ref = NULL;
		rk_smart_ir_result_t ir_res;
		XCamReturn ret;

		memset(&ir_res, 0, sizeof(ir_res));
		ret = rk_aiq_uapi2_sysctl_get3AStatsBlk(aiq_ctx, &stats_ref,
							200);
		if (ret != XCAM_RETURN_NO_ERROR || !stats_ref) {
			continue;
		}

		ret = rk_smart_ir_runOnce(smartIr_ctx, stats_ref, &ir_res);
		rk_aiq_uapi2_sysctl_release3AStatsRef(aiq_ctx, stats_ref);
		if (ret != XCAM_RETURN_NO_ERROR) {
			continue;
		}

		smartIr_apply_status(ir_res.status);
	}

	return NULL;
}

static void smartIr_load_config(rk_smart_ir_params_t *params,
				RK_SMART_IR_STATUS_t *init_status)
{
	/* Defaults match /etc/smart_ir.ini shipped with this package. */
	params->d2n_envL_th = 0.04f;
	params->n2d_envL_th = 0.50f;
	params->rggain_base = 1.0f;
	params->bggain_base = 1.0f;
	params->awbgain_rad = 0.1f;
	params->awbgain_dis = 0.3f;
	params->switch_cnts_th = 50;
	*init_status = RK_SMART_IR_STATUS_DAY;
}

static void smartIr_start(void)
{
	rk_smart_ir_params_t params;
	rk_smart_ir_result_t init_res;
	RK_SMART_IR_STATUS_t init_status;

	memset(&params, 0, sizeof(params));
	memset(&init_res, 0, sizeof(init_res));
	smartIr_load_config(&params, &init_status);

	smartIr_ctx = rk_smart_ir_init(aiq_ctx);
	if (!smartIr_ctx) {
		fprintf(stderr, "SmartIR: rk_smart_ir_init failed\n");
		return;
	}

	rk_smart_ir_config(smartIr_ctx, &params);
	init_res.status = init_status;
	rk_smart_ir_set_status(smartIr_ctx, init_res);
	smartIr_last_status = init_status;

	smartIr_quit = 0;
	if (pthread_create(&smartIr_tid, NULL, smartIr_thread, NULL) != 0) {
		fprintf(stderr, "SmartIR: pthread_create failed\n");
		rk_smart_ir_deInit(smartIr_ctx);
		smartIr_ctx = NULL;
		return;
	}
	smartIr_started = 1;
}

static void smartIr_stop(void)
{
	if (smartIr_started) {
		smartIr_quit = 1;
		pthread_join(smartIr_tid, NULL);
		smartIr_started = 0;
	}
	if (!smartIr_ctx) {
		return;
	}
	rk_smart_ir_deInit(smartIr_ctx);
	smartIr_ctx = NULL;
}

static void print_usage(const char *program_name)
{
	printf("Usage: %s [OPTIONS]\n", program_name);
	printf("Options:\n");
	printf("  -m, --mode MODE    Set day/night mode (day or night, "
	       "default: day)\n");
	printf("  -n, --no-smartir   Disable automatic day/night switching "
	       "via SmartIR\n");
	printf("  -d, --subdev PATH  Sensor V4L2 subdev for AE controls "
	       "(default: auto)\n");
	printf("  -h, --help         Show this help message\n");
}

int main(int argc, char *argv[])
{
	const char *mode = "day";
	const char *subdev_path = NULL;
	int enable_smartir = 1;
	int width = DISP_WIDTH;
	int height = DISP_HEIGHT;
	int vir_width = RK_ALIGN(width, 16);
	int opt;
	int font_scale;
	float fps = 0;
	char fps_text[16];
	char mode_text[32];
	char exp_text[32];
	char gain_text[32];
	unsigned int fps_frame_count = 0;
	RK_U64 fps_last_update_us = 0;
	RK_S32 s32Ret = 0;
	RK_U32 h264_time_ref = 0;
	int v4l2_subdev_fd = -1;
	int exposure = 0;
	int analogue_gain = 0;
	struct option long_options[] = {
		{"mode", required_argument, 0, 'm'},
		{"no-smartir", no_argument, 0, 'n'},
		{"subdev", required_argument, 0, 'd'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0},
	};
	VENC_STREAM_S stFrame;
	VIDEO_FRAME_INFO_S stViFrame;
	VIDEO_FRAME_INFO_S h264_frame;
	MB_POOL_CONFIG_S pool_cfg;
	MB_POOL src_pool;
	MB_BLK src_blk;
	VI_DEV_ATTR_S vi_dev_attr;
	VI_DEV_BIND_PIPE_S vi_bind_pipe;
	VI_CHN_ATTR_S vi_chn_attr;
	VENC_RECV_PIC_PARAM_S venc_recv_param;
	VENC_CHN_ATTR_S venc_attr;
	rtsp_demo_handle rtsplive = NULL;
	rtsp_session_handle rtsp_session;
	rga_buffer_t dst_img;
	unsigned char *data;

	while ((opt = getopt_long(argc, argv, "m:hnd:", long_options, NULL)) !=
	       -1) {
		switch (opt) {
		case 'm':
			if (strcmp(optarg, "day") == 0 ||
			    strcmp(optarg, "night") == 0) {
				mode = optarg;
			} else {
				fprintf(stderr,
					"Error: Invalid mode '%s'. Use 'day' "
					"or 'night'.\n",
					optarg);
				print_usage(argv[0]);
				return -1;
			}
			break;
		case 'n':
			enable_smartir = 0;
			break;
		case 'd':
			subdev_path = optarg;
			break;
		case 'h':
			print_usage(argv[0]);
			return 0;
		default:
			print_usage(argv[0]);
			return -1;
		}
	}

	font_scale = height / 320;
	if (font_scale < 2) {
		font_scale = 2;
	}

	stFrame.pstPack = malloc(sizeof(VENC_PACK_S));

	memset(&pool_cfg, 0, sizeof(pool_cfg));
	pool_cfg.u64MBSize = (RK_U64)vir_width * height * 3;
	pool_cfg.u32MBCnt = 1;
	pool_cfg.enAllocType = MB_ALLOC_TYPE_DMA;
	src_pool = RK_MPI_MB_CreatePool(&pool_cfg);
	src_blk = RK_MPI_MB_GetMB(src_pool, vir_width * height * 3, RK_TRUE);

	memset(&h264_frame, 0, sizeof(h264_frame));
	h264_frame.stVFrame.u32Width = width;
	h264_frame.stVFrame.u32Height = height;
	h264_frame.stVFrame.u32VirWidth = vir_width;
	h264_frame.stVFrame.u32VirHeight = height;
	h264_frame.stVFrame.enPixelFormat = RK_FMT_RGB888;
	h264_frame.stVFrame.u32FrameFlag = 160;
	h264_frame.stVFrame.pMbBlk = src_blk;
	data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_blk);
	dst_img = wrapbuffer_fd(RK_MPI_MB_Handle2Fd(src_blk), width, height,
				RK_FORMAT_BGR_888);

	rk_aiq_uapi2_sysctl_enumStaticMetas(0, &aiq_static_info);
	aiq_ctx = rk_aiq_uapi2_sysctl_init(
		aiq_static_info.sensor_info.sensor_name, "/etc/iqfiles", NULL,
		NULL);
	if (rk_aiq_uapi2_sysctl_prepare(aiq_ctx, 0, 0,
					RK_AIQ_WORKING_MODE_NORMAL)) {
		RK_LOGE("rk_aiq_uapi_sysctl_prepare fail!");
		return -1;
	}
	if (rk_aiq_uapi2_sysctl_start(aiq_ctx)) {
		RK_LOGE("rk_aiq_uapi_sysctl_start fail!");
		return -1;
	}
	rk_aiq_uapi2_sysctl_switch_scene(aiq_ctx, "normal", mode);
	scene_mode = mode;
	if (enable_smartir) {
		smartIr_start();
	}

	v4l2_subdev_fd = v4l2_open_sensor_subdev(subdev_path);
	if (v4l2_subdev_fd < 0) {
		fprintf(stderr,
			"Warning: exposure/gain overlay disabled\n");
	}

	if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		RK_LOGE("rk mpi sys init fail!");
		return -1;
	}

	rtsplive = create_rtsp_demo(554);
	rtsp_session = rtsp_new_session(rtsplive, "/live/0");
	rtsp_set_video(rtsp_session, RTSP_CODEC_ID_VIDEO_H265, NULL, 0);
	rtsp_sync_video_ts(rtsp_session, rtsp_get_reltime(),
			   rtsp_get_ntptime());

	memset(&vi_dev_attr, 0, sizeof(vi_dev_attr));
	memset(&vi_bind_pipe, 0, sizeof(vi_bind_pipe));
	s32Ret = RK_MPI_VI_GetDevAttr(0, &vi_dev_attr);
	if (s32Ret == RK_ERR_VI_NOT_CONFIG) {
		RK_MPI_VI_SetDevAttr(0, &vi_dev_attr);
	}
	if (RK_MPI_VI_GetDevIsEnable(0) != RK_SUCCESS) {
		RK_MPI_VI_EnableDev(0);
		vi_bind_pipe.u32Num = 1;
		vi_bind_pipe.PipeId[0] = 0;
		RK_MPI_VI_SetDevBindPipe(0, &vi_bind_pipe);
	}

	memset(&vi_chn_attr, 0, sizeof(vi_chn_attr));
	vi_chn_attr.stIspOpt.u32BufCount = 2;
	vi_chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	vi_chn_attr.stSize.u32Width = width;
	vi_chn_attr.stSize.u32Height = height;
	vi_chn_attr.enPixelFormat = RK_FMT_YUV420SP;
	vi_chn_attr.enCompressMode = COMPRESS_MODE_NONE;
	vi_chn_attr.u32Depth = 2;
	RK_MPI_VI_SetChnAttr(0, 0, &vi_chn_attr);
	RK_MPI_VI_EnableChn(0, 0);

	memset(&venc_attr, 0, sizeof(venc_attr));
	venc_attr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
	venc_attr.stRcAttr.stH265Cbr.u32BitRate = 10 * 1024;
	venc_attr.stRcAttr.stH265Cbr.u32Gop = 60;
	venc_attr.stVencAttr.enType = RK_VIDEO_ID_HEVC;
	venc_attr.stVencAttr.enPixelFormat = RK_FMT_RGB888;
	venc_attr.stVencAttr.u32PicWidth = width;
	venc_attr.stVencAttr.u32PicHeight = height;
	venc_attr.stVencAttr.u32VirWidth = vir_width;
	venc_attr.stVencAttr.u32VirHeight = height;
	venc_attr.stVencAttr.u32StreamBufCnt = 2;
	venc_attr.stVencAttr.u32BufSize = vir_width * height * 3 / 2;
	venc_attr.stVencAttr.enMirror = MIRROR_NONE;
	RK_MPI_VENC_CreateChn(0, &venc_attr);
	memset(&venc_recv_param, 0, sizeof(venc_recv_param));
	venc_recv_param.s32RecvPicNum = -1;
	RK_MPI_VENC_StartRecvFrame(0, &venc_recv_param);

	while (1) {
		h264_frame.stVFrame.u32TimeRef = h264_time_ref++;
		h264_frame.stVFrame.u64PTS = get_now_us();
		s32Ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
		if (s32Ret == RK_SUCCESS) {
			rga_buffer_t src_img = wrapbuffer_fd(
				RK_MPI_MB_Handle2Fd(stViFrame.stVFrame.pMbBlk),
				width, height, RK_FORMAT_YCrCb_420_SP);
			IM_STATUS cvt_ret = imcvtcolor(src_img, dst_img,
						       RK_FORMAT_YCrCb_420_SP,
						       RK_FORMAT_BGR_888);

			if (cvt_ret != IM_STATUS_SUCCESS) {
				RK_LOGE("imcvtcolor fail: %s",
					imStrError(cvt_ret));
			}

			snprintf(fps_text, sizeof(fps_text), "fps = %.1f", fps);
			snprintf(mode_text, sizeof(mode_text), "mode = %-5s",
				 scene_mode);
			if (v4l2_subdev_fd >= 0 &&
			    v4l2_read_ae(v4l2_subdev_fd, &exposure,
					 &analogue_gain) == 0) {
				snprintf(exp_text, sizeof(exp_text), "exp = %d",
					 exposure);
				snprintf(gain_text, sizeof(gain_text),
					 "gain = %d", analogue_gain);
			} else {
				snprintf(exp_text, sizeof(exp_text), "exp = ---");
				snprintf(gain_text, sizeof(gain_text),
					 "gain = ---");
			}
			draw_string(data, vir_width, width, height, 40, 40,
				    fps_text, font_scale, 0, 255, 0);
			draw_string(data, vir_width, width, height, 40,
				    40 + 10 * font_scale, mode_text,
				    font_scale, 0, 255, 255);
			draw_string(data, vir_width, width, height, 40,
				    40 + 20 * font_scale, exp_text,
				    font_scale, 255, 255, 0);
			draw_string(data, vir_width, width, height, 40,
				    40 + 30 * font_scale, gain_text,
				    font_scale, 255, 255, 0);
			RK_MPI_SYS_MmzFlushCache(src_blk, RK_FALSE);
		}

		RK_MPI_VENC_SendFrame(0, &h264_frame, -1);
		s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);
		if (s32Ret == RK_SUCCESS) {
			if (rtsplive && rtsp_session) {
				void *pData = RK_MPI_MB_Handle2VirAddr(
					stFrame.pstPack->pMbBlk);

				rtsp_tx_video(rtsp_session, pData,
					      stFrame.pstPack->u32Len,
					      stFrame.pstPack->u64PTS);
				rtsp_do_event(rtsplive);
			}

			fps_frame_count++;
			RK_U64 now_us = get_now_us();

			if (fps_last_update_us == 0) {
				fps_last_update_us = now_us;
			}

			RK_U64 elapsed = now_us - fps_last_update_us;

			if (elapsed >= 1000000) {
				fps = (float)fps_frame_count * 1000000.f /
				      (float)elapsed;
				fps_frame_count = 0;
				fps_last_update_us = now_us;
			}
		}

		RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
		RK_MPI_VENC_ReleaseStream(0, &stFrame);
	}

	RK_MPI_MB_ReleaseMB(src_blk);
	RK_MPI_MB_DestroyPool(src_pool);
	RK_MPI_VI_DisableChn(0, 0);
	RK_MPI_VI_DisableDev(0);
	RK_MPI_VENC_StopRecvFrame(0);
	RK_MPI_VENC_DestroyChn(0);
	free(stFrame.pstPack);
	if (rtsplive) {
		rtsp_del_demo(rtsplive);
	}
	RK_MPI_SYS_Exit();
	if (v4l2_subdev_fd >= 0) {
		close(v4l2_subdev_fd);
	}
	smartIr_stop();
	if (aiq_ctx) {
		rk_aiq_uapi2_sysctl_stop(aiq_ctx, false);
		rk_aiq_uapi2_sysctl_deinit(aiq_ctx);
		aiq_ctx = NULL;
	}

	return 0;
}
