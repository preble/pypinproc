/**
 * Copyright (c) 2009-2011 Adam Preble and Gerry Stellenberg
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 **
 * 
 * DMD Library
 * 
 * This library (dmd.h and dmd.c) provide functions for managing and manipulating
 * bitmaps that represent DMD (dot matrix display) frame buffers.  The functions
 * here are based on the DMDBuffer class interface in pypinproc, and have been
 * adapted here for use in other contexts with an emphasis on portability and
 * zero dependencies.
 * 
 * - Adam Preble, March 2011
 */

#ifndef _DMD_H_
#define _DMD_H_

#if defined(__cplusplus)
    #define DMD_EXTERN_C_BEGIN extern "C" {
    #define DMD_EXTERN_C_END   }
#else
    #define DMD_EXTERN_C_BEGIN
    #define DMD_EXTERN_C_END
#endif

DMD_EXTERN_C_BEGIN

typedef unsigned char DMDColor;

/**
 * Geometry Types
 * 
 * DMDRect, DMDSize, DMDPoint data structures adapted from Apple's Core Graphics data structures.
 */

typedef struct _DMDPoint {
	unsigned x, y;
} DMDPoint;

typedef struct _DMDSize {
	unsigned width, height;
} DMDSize;

typedef struct _DMDRect {
	DMDPoint origin;
	DMDSize size;
} DMDRect;

static inline DMDPoint DMDPointMake(unsigned x, unsigned y) { DMDPoint p = {x, y}; return p; }
static inline DMDSize DMDSizeMake(unsigned w, unsigned h) { DMDSize s = {w, h}; return s; }

static inline DMDRect DMDRectMake(unsigned x, unsigned y, unsigned w, unsigned h) { DMDRect r = {{x, y}, {w, h}}; return r; }
static inline unsigned DMDRectGetMinX(DMDRect r) { return r.origin.x; }
static inline unsigned DMDRectGetMinY(DMDRect r) { return r.origin.y; }
static inline unsigned DMDRectGetMaxX(DMDRect r) { return r.origin.x + r.size.width; }
static inline unsigned DMDRectGetMaxY(DMDRect r) { return r.origin.y + r.size.height; }

DMDRect DMDRectIntersection(DMDRect a, DMDRect b);


/**
 * DMDFrame - Creation/Management
 */

typedef struct _DMDFrame {
	DMDSize size;
	DMDColor *buffer;
} DMDFrame;

DMDFrame *DMDFrameCreate(DMDSize size);
void DMDFrameDelete(DMDFrame *frame);
DMDFrame *DMDFrameCopy(DMDFrame *frame);


/**
 * DMDFrame - Accessors
 */

DMDRect DMDFrameGetBounds(DMDFrame *frame);
DMDSize DMDFrameGetSize(DMDFrame *frame);
unsigned DMDFrameGetBufferSize(DMDFrame *frame);
static inline DMDColor *DMDFrameGetDotPointer(DMDFrame *frame, DMDPoint p) { return &frame->buffer[p.y * frame->size.width + p.x]; }
static inline DMDColor DMDFrameGetDot(DMDFrame *frame, DMDPoint p) { return frame->buffer[p.y * frame->size.width + p.x]; }
static inline void DMDFrameSetDot(DMDFrame *frame, DMDPoint p, DMDColor c) { frame->buffer[p.y * frame->size.width + p.x] = c; }

/**
 * DMDFrame - Manipulation
 */

void DMDFrameFillRect(DMDFrame *frame, DMDRect rect, DMDColor color);

typedef enum {
	DMDBlendModeCopy = 0,
	DMDBlendModeAdd = 1,
	DMDBlendModeSubtract = 2,
	DMDBlendModeBlackSource = 3,
	DMDBlendModeAlpha = 4,
	DMDBlendModeAlphaBoth = 5,
} DMDBlendMode;

void DMDFrameCopyRect(DMDFrame *from, DMDRect fromRect, DMDFrame *to, DMDPoint toPoint, DMDBlendMode blendMode);


/**
 * DMDFrame - P-ROC DMD Driver Support
 */

void DMDFrameCopyPROCSubframes(DMDFrame *frame, unsigned char *dots, unsigned width, unsigned height, unsigned subframes, unsigned char *colorMap);


DMD_EXTERN_C_END

#endif 
/* _DMD_H_ */
