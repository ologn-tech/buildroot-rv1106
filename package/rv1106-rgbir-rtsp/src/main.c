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

RK_U64 TEST_COMM_GetNowUs()
{
	struct timespec time = { 0, 0 };
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}

struct buffer {
	void *start;
	size_t length;
};

static inline void xform4x4_tile(const uint8_t *s0, const uint8_t *s1, const uint8_t *s2, const uint8_t *s3, uint8_t *d0, uint8_t *d1,
			  uint8_t *d2, uint8_t *d3)
{
	// Load the 4×4 once (t0..t15)
	uint8_t t0 = s0[0], t1 = s0[1], t2 = s0[2], t3 = s0[3];
	uint8_t t4 = s1[0], t5 = s1[1], t6 = s1[2], t7 = s1[3];
	uint8_t t8 = s2[0], t9 = s2[1], t10 = s2[2], t11 = s2[3];
	uint8_t t12 = s3[0], t13 = s3[1], t14 = s3[2], t15 = s3[3];

	// Mapping derived from your if/else:
	// [0,1,0,3, 4,2,6,2, 0,9,10,11, 12,2,14,2]
	d0[0] = t0;
	d0[1] = t1;
	d0[2] = t0;
	d0[3] = t3;
	d1[0] = t4;
	d1[1] = t2;
	d1[2] = t6;
	d1[3] = t2;
	d2[0] = t0;
	d2[1] = t9;
	d2[2] = t10;
	d2[3] = t11;
	d3[0] = t12;
	d3[1] = t2;
	d3[2] = t14;
	d3[3] = t2;
}

static inline void rgbir_to_bggr(uint8_t *dst, uint8_t *src, int w, int h)
{
	// assume w=1792, h=1296, both multiples of 4
	const int w4 = w; // already 4-aligned
	const int h4 = h; // already 4-aligned

	for (int y = 0; y < h4; y += 4) {
		const uint8_t *s0 = src + y * w;
		const uint8_t *s1 = s0 + w;
		const uint8_t *s2 = s1 + w;
		const uint8_t *s3 = s2 + w;

		uint8_t *d0 = dst + y * w;
		uint8_t *d1 = d0 + w;
		uint8_t *d2 = d1 + w;
		uint8_t *d3 = d2 + w;

		// prefetch a couple stripes ahead (helps on A7)
		if (y + 16 < h4) {
			__builtin_prefetch(src + (y + 16) * w, 0, 1);
			__builtin_prefetch(dst + (y + 16) * w, 1, 1);
		}

		// process tiles horizontally; unroll a bit to reduce loop overhead
		int x = 0;
		for (; x + 16 <= w4; x += 16) {
			__builtin_prefetch(s0 + x + 64, 0, 1);
			__builtin_prefetch(s1 + x + 64, 0, 1);
			__builtin_prefetch(s2 + x + 64, 0, 1);
			__builtin_prefetch(s3 + x + 64, 0, 1);

			xform4x4_tile(s0 + x + 0, s1 + x + 0, s2 + x + 0, s3 + x + 0, d0 + x + 0, d1 + x + 0, d2 + x + 0,
				      d3 + x + 0);
			xform4x4_tile(s0 + x + 4, s1 + x + 4, s2 + x + 4, s3 + x + 4, d0 + x + 4, d1 + x + 4, d2 + x + 4,
				      d3 + x + 4);
			xform4x4_tile(s0 + x + 8, s1 + x + 8, s2 + x + 8, s3 + x + 8, d0 + x + 8, d1 + x + 8, d2 + x + 8,
				      d3 + x + 8);
			xform4x4_tile(s0 + x + 12, s1 + x + 12, s2 + x + 12, s3 + x + 12, d0 + x + 12, d1 + x + 12, d2 + x + 12,
				      d3 + x + 12);
		}
		for (; x < w4; x += 4) {
			xform4x4_tile(s0 + x, s1 + x, s2 + x, s3 + x, d0 + x, d1 + x, d2 + x, d3 + x);
		}
	}
}

int main(int argc, char *argv[])
{
	RK_S32 s32Ret = 0;

	int width = WIDTH;
	int height = HEIGHT;

	uint8_t *buf_in = (uint8_t *)malloc(width * height);
	uint8_t *buf_in_bggr = (uint8_t *)malloc(width * height);

	char fps_text[16];
	float fps = 0;
	memset(fps_text, 0, 16);

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
	fmt.fmt.pix_mp.num_planes = 1; // For BG10
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		perror("Setting Pixel Format");
		close(fd);
		return 1;
	}

	// Print actual format details
	printf("Actual format: %dx%d, pixelformat: %.4s\n", fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
	       (char *)&fmt.fmt.pix_mp.pixelformat);

	// Request buffers
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count = BUFFER_COUNT;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		perror("Requesting Buffer");
		close(fd);
		return 1;
	}

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
		if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
			perror("Querying Buffer");
			close(fd);
			return 1;
		}
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
		if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			perror("Queue Buffer");
			close(fd);
			return 1;
		}
	}

	// Start streaming
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
		perror("Start Capture");
		close(fd);
		return 1;
	}
	printf("V4L2 initialized successfully\n");

	while (1) {
		h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
		h264_frame.stVFrame.u64PTS = TEST_COMM_GetNowUs();

		struct v4l2_buffer buf;
		struct v4l2_plane planes[VIDEO_MAX_PLANES];
		memset(&buf, 0, sizeof(buf));
		memset(planes, 0, sizeof(planes));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.length = 1; // Number of planes
		buf.m.planes = planes;

		if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
			perror("Retrieving Frame");
			break;
		}

		size_t data_size = planes[0].bytesused;
		size_t max_size = width * height;
		if (data_size > max_size) {
			data_size = max_size; // Limit to expected size
		}
		memcpy(buf_in, buffers[buf.index].start, data_size);

		if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			perror("Requeue Buffer");
			break;
		}

		rgbir_to_bggr(buf_in_bggr, buf_in, width, height);

		struct mpix_image img;
		struct mpix_format fmt = { .fourcc = MPIX_FMT_SBGGR8, .width = width, .height = height };
		mpix_image_from_buf(&img, buf_in_bggr, width * height, &fmt);
		mpix_image_debayer(&img, 2);
		mpix_image_correct_white_balance(&img);
		mpix_image_ctrl_value(&img, MPIX_CID_RED_BALANCE, 1.2 * (1 << 10));
		mpix_image_ctrl_value(&img, MPIX_CID_BLUE_BALANCE, 1.55 * (1 << 10));
		mpix_image_to_buf(&img, data, width * height * 3);

		mpix_print_pipeline(img.first_op);

		mpix_image_free(&img);

		sprintf(fps_text, "fps = %.2f", fps);
		printf("fps = %.2f\n", fps);

		// send stream
		// encode H264
		RK_MPI_VENC_SendFrame(0, &h264_frame, -1);

		// rtsp
		s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);
		if (s32Ret == RK_SUCCESS) {
			if (g_rtsplive && g_rtsp_session) {
				void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
				rtsp_tx_video(g_rtsp_session, (uint8_t *)pData, stFrame.pstPack->u32Len, stFrame.pstPack->u64PTS);
				rtsp_do_event(g_rtsplive);
			}
			RK_U64 nowUs = TEST_COMM_GetNowUs();
			fps = (float)1000000 / (float)(nowUs - h264_frame.stVFrame.u64PTS);
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
	free(buf_in);

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
