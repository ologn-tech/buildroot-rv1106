#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#include <rk_mpi_mb.h>
#include <rk_mpi_venc.h>
#include <rk_mpi_vi.h>
#include <rk_mpi_vpss.h>
#include <rk_mpi_sys.h>
#include <rtsp_demo.h>

#include <mpix/image.h>

#define WIDTH 1536
#define HEIGHT 1296
#define DEVICE "/dev/video0"
#define BUFFER_COUNT 1

struct buffer {
	void *start;
	size_t length;
};

static void xioctl(int fh, int request, void *arg)
{
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

	if (r == -1) {
		fprintf(stderr, "error %d, %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

RK_U64 get_now_us(void)
{
	struct timespec time = { 0, 0 };
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}

/*
	Input			Output
	B   G   R   G		B   G   B   G
	G  IR   G  IR		G   R   G   R
	R   G   B   G		B   G   B   G
	G  IR   G  IR		G   R   G   R
*/
static inline void xform4x4_tile(uint8_t *s0, uint8_t *s1, uint8_t *s2, uint8_t *s3, uint8_t *d0, uint8_t *d1, uint8_t *d2,
				 uint8_t *d3)
{
	uint8_t t00 = s0[0], t01 = s0[1], t02 = s0[2], t03 = s0[3];
	uint8_t t04 = s1[0], t05 = s1[1], t06 = s1[2], t07 = s1[3];
	uint8_t t08 = s2[0], t09 = s2[1], t10 = s2[2], t11 = s2[3];
	uint8_t t12 = s3[0], t13 = s3[1], t14 = s3[2], t15 = s3[3];

	(void)t05;
	(void)t07;
	(void)t13;
	(void)t15;

	d0[0] = t00;
	d0[1] = t01;
	d0[2] = t00;
	d0[3] = t03;
	d1[0] = t04;
	d1[1] = t02;
	d1[2] = t06;
	d1[3] = t02;
	d2[0] = t00;
	d2[1] = t09;
	d2[2] = t10;
	d2[3] = t11;
	d3[0] = t12;
	d3[1] = t08;
	d3[2] = t14;
	d3[3] = t08;
}

static inline void rgbir_to_bggr(uint8_t *src, uint8_t *dst, int width, int height)
{
	for (int y = 0; y < height; y += 4) {
		uint8_t *s0 = src + y * width;
		uint8_t *s1 = s0 + width;
		uint8_t *s2 = s1 + width;
		uint8_t *s3 = s2 + width;

		uint8_t *d0 = dst + y * width;
		uint8_t *d1 = d0 + width;
		uint8_t *d2 = d1 + width;
		uint8_t *d3 = d2 + width;

		for (int x = 0; x < width; x += 4) {
			xform4x4_tile(s0 + x, s1 + x, s2 + x, s3 + x, d0 + x, d1 + x, d2 + x, d3 + x);
		}
	}
}

int main(void)
{
	int width = WIDTH;
	int height = HEIGHT;

	uint8_t *buffer = (uint8_t *)malloc(width * height);

	// rkmpi init
	if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		RK_LOGE("rk mpi sys init fail!");
		return -1;
	}

	// h264_frame
	VENC_STREAM_S stFrame;
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
	RK_U32 H264_TimeRef = 0;

	// Create Pool
	MB_POOL_CONFIG_S PoolCfg;
	memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
	PoolCfg.u64MBSize = width * height * 3;
	PoolCfg.u32MBCnt = 1;
	PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;
	MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg);
	printf("Create Pool success !\n");

	// Get MB from Pool
	MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, width * height * 3, RK_TRUE);

	// Build H264 frame
	VIDEO_FRAME_INFO_S h264_frame;
	h264_frame.stVFrame.u32Width = width;
	h264_frame.stVFrame.u32Height = height;
	h264_frame.stVFrame.u32VirWidth = width;
	h264_frame.stVFrame.u32VirHeight = height;
	h264_frame.stVFrame.enPixelFormat = RK_FMT_RGB888;
	h264_frame.stVFrame.u32FrameFlag = 160;
	h264_frame.stVFrame.pMbBlk = src_Blk;
	unsigned char *data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);

	// RTSP init
	rtsp_demo_handle g_rtsplive = NULL;
	rtsp_session_handle g_rtsp_session;
	g_rtsplive = create_rtsp_demo(554);
	g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
	rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H265, NULL, 0);
	rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());

	// VENC init
	VENC_RECV_PIC_PARAM_S stRecvParam;
	VENC_CHN_ATTR_S stAttr;
	memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));
	stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
	stAttr.stRcAttr.stH265Cbr.u32BitRate = 10 * 1024;
	stAttr.stRcAttr.stH265Cbr.u32Gop = 60;
	stAttr.stVencAttr.enType = RK_VIDEO_ID_HEVC;
	stAttr.stVencAttr.enPixelFormat = RK_FMT_RGB888;
	stAttr.stVencAttr.u32PicWidth = width;
	stAttr.stVencAttr.u32PicHeight = height;
	stAttr.stVencAttr.u32VirWidth = width;
	stAttr.stVencAttr.u32VirHeight = height;
	stAttr.stVencAttr.u32StreamBufCnt = 2;
	stAttr.stVencAttr.u32BufSize = width * height * 3 / 2;
	stAttr.stVencAttr.enMirror = MIRROR_NONE;
	RK_MPI_VENC_CreateChn(0, &stAttr);
	memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
	stRecvParam.s32RecvPicNum = -1;
	RK_MPI_VENC_StartRecvFrame(0, &stRecvParam);

	int fd = open(DEVICE, O_RDWR);
	if (fd < 0) {
		perror("Cannot open device");
		return 1;
	}

	// Set format
	struct v4l2_format fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width = width;
	fmt.fmt.pix_mp.height = height;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SBGGR8;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.num_planes = 1;
	xioctl(fd, VIDIOC_S_FMT, &fmt);

	// Request buffers
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count = BUFFER_COUNT;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = V4L2_MEMORY_MMAP;
	xioctl(fd, VIDIOC_REQBUFS, &req);

	// Map buffers
	struct buffer buffers[BUFFER_COUNT];
	for (int i = 0; i < BUFFER_COUNT; ++i) {
		struct v4l2_buffer buf;
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.length = 1; // Number of planes
		buf.m.planes = planes;
		xioctl(fd, VIDIOC_QUERYBUF, &buf);

		buffers[i].length = planes[0].length;
		buffers[i].start = mmap(NULL, planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, planes[0].m.mem_offset);
		if (buffers[i].start == MAP_FAILED) {
			perror("mmap");
			close(fd);
			return 1;
		}
	}

	// Queue buffers
	for (int i = 0; i < BUFFER_COUNT; ++i) {
		struct v4l2_buffer buf;
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.length = 1; // Number of planes
		buf.m.planes = planes;
		xioctl(fd, VIDIOC_QBUF, &buf);
	}

	// Start streaming
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	xioctl(fd, VIDIOC_STREAMON, &type);

	printf("V4L2 initialized successfully\n");

	int frame_count = 0;

	while (1) {
		h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
		h264_frame.stVFrame.u64PTS = get_now_us();

		struct v4l2_buffer buf;
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.length = 1; // Number of planes
		buf.m.planes = planes;

		xioctl(fd, VIDIOC_DQBUF, &buf);

		rgbir_to_bggr(buffers[buf.index].start, buffer, width, height);

		xioctl(fd, VIDIOC_QBUF, &buf);

		struct mpix_image img;
		struct mpix_format fmt = { .fourcc = MPIX_FMT_SBGGR8, .width = width, .height = height };
		mpix_image_from_buf(&img, buffer, width * height, &fmt);
		mpix_image_debayer(&img, 2);
		mpix_image_correct_white_balance(&img);
		mpix_image_ctrl_value(&img, MPIX_CID_RED_BALANCE, 1.2 * (1 << 10));
		mpix_image_ctrl_value(&img, MPIX_CID_BLUE_BALANCE, 1.55 * (1 << 10));
		mpix_image_to_buf(&img, data, width * height * 3);

		mpix_image_free(&img);

		// send stream
		// encode H264
		RK_MPI_VENC_SendFrame(0, &h264_frame, -1);

		// rtsp
		RK_S32 s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);
		if (s32Ret == RK_SUCCESS) {
			if (g_rtsplive && g_rtsp_session) {
				void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
				rtsp_tx_video(g_rtsp_session, (uint8_t *)pData, stFrame.pstPack->u32Len, stFrame.pstPack->u64PTS);
				rtsp_do_event(g_rtsplive);
			}
			RK_U64 nowUs = get_now_us();
			float fps = (float)1000000 / (float)(nowUs - h264_frame.stVFrame.u64PTS);

			// Print FPS only every 10 frames
			frame_count++;
			if (frame_count % 10 == 0) {
				printf("\rFPS: %.1f", fps);
				fflush(stdout);
			}
		}

		s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
		if (s32Ret != RK_SUCCESS) {
			RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
		}
	}

	// Destory MB
	RK_MPI_MB_ReleaseMB(src_Blk);
	// Destory Pool
	RK_MPI_MB_DestroyPool(src_Pool);

	// Free allocated buffers
	free(buffer);

	// Unmap buffers
	for (int i = 0; i < BUFFER_COUNT; ++i) {
		munmap(buffers[i].start, buffers[i].length);
	}

	close(fd);

	RK_MPI_VENC_StopRecvFrame(0);
	RK_MPI_VENC_DestroyChn(0);

	free(stFrame.pstPack);

	if (g_rtsplive) {
		rtsp_del_demo(g_rtsplive);
	}

	RK_MPI_SYS_Exit();

	return 0;
}
