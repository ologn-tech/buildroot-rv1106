#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <time.h>
#include <unistd.h>

#include <rk_mpi_mb.h>
#include <rk_mpi_venc.h>
#include <rk_mpi_vi.h>
#include <rk_mpi_vpss.h>
#include <rk_mpi_sys.h>
#include "rtsp_demo.h"

#include <mpix/image.h>

#define DEVICE       "/dev/video0"
#define BUFFER_COUNT 4

#define WIDTH (((VIDEO_WIDTH + 255) / 256) * 256)
#define HEIGHT VIDEO_HEIGHT

RK_U64 TEST_COMM_GetNowUs() {
	struct timespec time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}

int venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType) {
	printf("%s\n",__func__);
	VENC_RECV_PIC_PARAM_S stRecvParam;
	VENC_CHN_ATTR_S stAttr;
	memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));

	if (enType == RK_VIDEO_ID_AVC) {
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
		stAttr.stRcAttr.stH264Cbr.u32BitRate = 10 * 1024;
		stAttr.stRcAttr.stH264Cbr.u32Gop = 1;
	} else if (enType == RK_VIDEO_ID_HEVC) {
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
		stAttr.stRcAttr.stH265Cbr.u32BitRate = 10 * 1024;
		stAttr.stRcAttr.stH265Cbr.u32Gop = 60;
	} else if (enType == RK_VIDEO_ID_MJPEG) {
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
		stAttr.stRcAttr.stMjpegCbr.u32BitRate = 10 * 1024;
	}

	stAttr.stVencAttr.enType = enType;
	stAttr.stVencAttr.enPixelFormat = RK_FMT_RGB888;
	if (enType == RK_VIDEO_ID_AVC)
		stAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;
	stAttr.stVencAttr.u32PicWidth = width;
	stAttr.stVencAttr.u32PicHeight = height;
	stAttr.stVencAttr.u32VirWidth = width;
	stAttr.stVencAttr.u32VirHeight = height;
	stAttr.stVencAttr.u32StreamBufCnt = 2;
	stAttr.stVencAttr.u32BufSize = width * height * 3 / 2;
	stAttr.stVencAttr.enMirror = MIRROR_NONE;

	RK_MPI_VENC_CreateChn(chnId, &stAttr);

	memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
	stRecvParam.s32RecvPicNum = -1;
	RK_MPI_VENC_StartRecvFrame(chnId, &stRecvParam);

	return 0;
}

struct buffer {
	void *start;
	size_t length;
};

static void build_bggr_from_rgbir4x4(uint8_t *src, uint8_t *dst, int w, int h)
{
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			int bx = (x & ~3), by = (y & ~3); // gốc block 4x4
			int lx = x & 3, ly = y & 3;       // local trong block

			int sx = lx, sy = ly; // mặc định

			if ((x & 1) == 1 && (y & 1) == 1) {
				// RED (odd,odd) → IR thành RED
				// lấy từ RED cũ (0,2) hoặc (2,0), ví dụ chọn (2,0)
				sx = 2;
				sy = 0;
			} else if ((x & 1) == 0 && (y & 1) == 0) {
				// BLUE (even,even)
				if ((lx == 0 && ly == 0) || (lx == 2 && ly == 2)) {
					// Blue cũ → giữ nguyên
				} else {
					// vị trí Red cũ (0,2) hoặc (2,0) → chuyển thành Blue
					sx = 0;
					sy = 0; // ví dụ luôn lấy từ Blue (0,0)
				}
			} else {
				// GREEN còn lại → giữ nguyên
			}

			dst[y * w + x] = src[(by + sy) * w + (bx + sx)];
		}
	}
}

int main(int argc, char *argv[])
{
	RK_S32 s32Ret = 0;

	int width = WIDTH;
	int height = HEIGHT;

	// SBGGR10 format: 10 bits per pixel, but we need to handle it as raw data
	// Allocate enough space for the raw capture data (typically 2 bytes per pixel for 10-bit)
	uint8_t *buf_in = (uint8_t *)malloc(height * width);
	uint8_t *buf_in_bggr = (uint8_t *)malloc(height * width);
	uint8_t *buf_out = (uint8_t *)malloc(height * width * 3);

	char fps_text[16];
	float fps = 0;
	memset(fps_text, 0, 16);
	RK_U64 nowUs;

	// rkmpi init
	if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		RK_LOGE("rk mpi sys init fail!");
		return -1;
	}

	// h264_frame
	VENC_STREAM_S stFrame;
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
	RK_U64 H264_PTS = 0;
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

	// Build h264_frame
	VIDEO_FRAME_INFO_S h264_frame;
	h264_frame.stVFrame.u32Width = width;
	h264_frame.stVFrame.u32Height = height;
	h264_frame.stVFrame.u32VirWidth = width;
	h264_frame.stVFrame.u32VirHeight = height;
	h264_frame.stVFrame.enPixelFormat = RK_FMT_RGB888;
	h264_frame.stVFrame.u32FrameFlag = 160;
	h264_frame.stVFrame.pMbBlk = src_Blk;
	unsigned char *data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);

	// rtsp init
	rtsp_demo_handle g_rtsplive = NULL;
	rtsp_session_handle g_rtsp_session;
	g_rtsplive = create_rtsp_demo(554);
	g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
	rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H265, NULL, 0);
	rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());

	// venc init
	RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_HEVC;
	venc_init(0, width, height, enCodecType);

	int fd = open(DEVICE, O_RDWR);
	if (fd < 0) {
		perror("Cannot open device");
		return 1;
	}

	// Set format
	v4l2_format fmt;
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
	printf("Actual format: %dx%d, pixelformat: %.4s\n", fmt.fmt.pix_mp.width,
	       fmt.fmt.pix_mp.height, (char *)&fmt.fmt.pix_mp.pixelformat);

	// Request buffers
	v4l2_requestbuffers req;
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
	buffer buffers[BUFFER_COUNT];
	for (int i = 0; i < BUFFER_COUNT; ++i) {
		v4l2_buffer buf;
		v4l2_plane planes[VIDEO_MAX_PLANES];
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
		buffers[i].start = mmap(NULL, planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED,
					fd, planes[0].m.mem_offset);
		if (buffers[i].start == MAP_FAILED) {
			perror("mmap");
			close(fd);
			return 1;
		}
	}

	// Queue buffers
	for (int i = 0; i < BUFFER_COUNT; ++i) {
		v4l2_buffer buf;
		v4l2_plane planes[VIDEO_MAX_PLANES];
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
	v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
		perror("Start Capture");
		close(fd);
		return 1;
	}
	printf("V4L2 initialized successfully\n");

	printf("init success\n");

	int frame_count = 0;
	while (1) {
		v4l2_buffer buf;
		v4l2_plane planes[VIDEO_MAX_PLANES];
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

		// Copy raw BG10 data to processing buffer
		// Note: SBGGR10 format has 10 bits per pixel, but we need to handle it properly
		// For now, we'll copy the raw data and let libmpix handle the format conversion
		size_t data_size = planes[0].bytesused;
		size_t max_size = width * height; // Maximum expected size for 10-bit data
		if (data_size > max_size) {
			data_size = max_size; // Limit to expected size
		}
		memcpy(buf_in, buffers[buf.index].start, data_size);

		if (frame_count % 30 == 0) {
			printf("Captured frame %d (%zu bytes)\n", frame_count + 1, data_size);
		}

		if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
			perror("Requeue Buffer");
			break;
		}

		// Save first frame to file for debugging
		static int first_frame_saved = 0;
		if (first_frame_saved == 0) {
			FILE *fp = fopen("first_frame.raw", "wb");
			if (fp) {
				fwrite(buf_in, 1, data_size, fp);
				fclose(fp);
				printf("Saved first frame to first_frame.raw (%zu bytes)\n",
				       data_size);
			}
			first_frame_saved = 1;
		}
		build_bggr_from_rgbir4x4(buf_in, buf_in_bggr, width, height);

		// Process image with libmpix
		struct mpix_image img;
		struct mpix_format fmt = { .fourcc = MPIX_FMT_SBGGR8, .width = width, .height = height };
		mpix_image_from_buf(&img, buf_in_bggr, width * height, &fmt);
		mpix_image_debayer(&img, 2);
		mpix_image_to_buf(&img, data, width * height * 3);

		frame_count++;

		// Set frame metadata
		h264_frame.stVFrame.u32TimeRef = H264_TimeRef++;
		h264_frame.stVFrame.u64PTS = TEST_COMM_GetNowUs();

		// send stream
		// encode H264
		RK_MPI_VENC_SendFrame(0, &h264_frame, -1);

		// rtsp
		s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);
		if (s32Ret == RK_SUCCESS) {
			if (g_rtsplive && g_rtsp_session) {
				void *pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
				rtsp_tx_video(g_rtsp_session, (uint8_t *)pData,
					      stFrame.pstPack->u32Len, stFrame.pstPack->u64PTS);
				rtsp_do_event(g_rtsplive);
			}
			nowUs = TEST_COMM_GetNowUs();
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
	free(buf_out);

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
