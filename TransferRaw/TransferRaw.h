#pragma once

#ifndef __TRANSFER_RAW_H__
#define __TRANSFER_RAW_H__

#ifndef USING_516
#define USING_516
#endif

#ifndef USING_316
//#define USING_316
#endif

#ifdef USING_516
#define IMG_WIDTH   640
#define IMG_HEIGHT  480
#define IMG_PHASE	4
#define EBD_LINE	3
#elif USING_316
#define IMG_WIDTH   240
#define IMG_HEIGHT  180
#define IMG_PHASE	4
#define EBD_LINE	2
#else
#error Please choose a sensor model: either IMX316 or IMX516.
#endif

#define FREC_100M	1000000
#define FREC_60M	600000
#define FREC		FREC_100M

#define C	299792458
#define PI	acos(-1)

/* save a, b group in 3 byte/pixel */
#define RAW_SIZE    IMG_WIDTH * ((IMG_HEIGHT + EBD_LINE) * IMG_PHASE) * 3

/* raw 12 saving size */
#define IMG_SIZE	IMG_WIDTH * (IMG_HEIGHT * IMG_PHASE) * 2

/* a frame size */
#define FRAME_SIZE	IMG_WIDTH * IMG_HEIGHT

/* embeded line range */
#define EBD_LINE_START(no)			IMG_WIDTH * (IMG_HEIGHT * (no) + EBD_LINE * ((no) - 1)) * 3
#define EBD_LINE_END(no)			IMG_WIDTH * (IMG_HEIGHT + EBD_LINE) * (no) * 3
#define EBD_LINE_RANGE(idx, no)		((idx) >= EBD_LINE_START((no)) && (idx) < EBD_LINE_END((no)))
#define IS_EBD_LINE(idx)			(EBD_LINE_RANGE((idx), 1) || EBD_LINE_RANGE((idx), 2) || EBD_LINE_RANGE((idx), 3) || EBD_LINE_RANGE((idx), 4))

/* 2 byte raw 12 transform */
#define RAW12(buf, idx)		(((int)(buf)[(idx)] >> 8) | (int)(buf)[(idx)+1])

/* jump to CONTINUE if function return false */
#ifdef  ASSERT
#undef  ASSERT
#endif
#define ASSERT(ex)	if (!(ex)) { goto CONTINUE; }

#endif	/*__TRANSFER_RAW_H__*/
