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
 */
#include "dmd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif /* MIN */
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif	/* MAX */


DMDColor *DMDGetAlphaMap(void); /** Private function for fetching the alpha map. */


DMDFrame *DMDFrameCreate(DMDSize size)
{
	/* Allocate the memory for the structure *and* the buffer at the same time.    *
	 * Note that this means we only free the DMDFrame pointer when the time comes. */
	unsigned char *ptr = (DMDColor *)calloc(sizeof(DMDFrame) + sizeof(DMDColor) * size.width * size.height, 1);
	if (ptr == NULL)
		return NULL;
	
	DMDFrame *frame = (DMDFrame *)ptr;
	frame->size = size;
	frame->buffer = (DMDColor *)(ptr + sizeof(DMDFrame));
	
	return frame;
}

void DMDFrameDelete(DMDFrame *frame)
{
	/* Only free the frame because it was allocated as one chunk of memory. */
	frame->buffer = NULL;
	free(frame);
}

DMDFrame *DMDFrameCopy(DMDFrame *frame)
{
	DMDFrame *copy = DMDFrameCreate(frame->size);
	memcpy(copy->buffer, frame->buffer, sizeof(DMDColor) * frame->size.width * frame->size.height);
	return copy;
}


DMDRect DMDFrameGetBounds(DMDFrame *frame)
{
	return DMDRectMake(0, 0, frame->size.width, frame->size.height);
}

DMDSize DMDFrameGetSize(DMDFrame *frame)
{
	return frame->size;
}

unsigned DMDFrameGetBufferSize(DMDFrame *frame)
{
	return frame->size.width * frame->size.height * sizeof(DMDColor);
}

void DMDFrameFillRect(DMDFrame *frame, DMDRect rect, DMDColor color)
{
	rect = DMDRectIntersection(DMDFrameGetBounds(frame), rect);
	
	DMDDimension maxX = DMDRectGetMaxX(rect);
	DMDDimension maxY = DMDRectGetMaxY(rect);
	
	DMDDimension x, y;
	
	/* TOOD: Rewrite this to use memset for rows. */
	for (x = DMDRectGetMinX(rect); x < maxX; x++)
		for (y = DMDRectGetMinY(rect); y < maxY; y++)
			frame->buffer[y * frame->size.width + x] = color;
}

void DMDFrameCopyRect(DMDFrame *src, DMDRect srcRect, DMDFrame *dst, DMDPoint dstPoint, DMDBlendMode blendMode)
{
	srcRect = DMDRectIntersection(DMDFrameGetBounds(src), srcRect);
	DMDRect dstRect = DMDRectIntersection(DMDFrameGetBounds(dst), DMDRectMake(dstPoint.x, dstPoint.y, srcRect.size.width, srcRect.size.height));
	// Short term fix for negative destination points:
	if (dstPoint.x < 0)
		srcRect.origin.x += -dstPoint.x;
	if (dstPoint.y < 0)
		srcRect.origin.y += -dstPoint.y;
	if (srcRect.size.width == 0 || srcRect.size.height == 0)
		return; /* nothing to do */
	
	DMDDimension width  = dstRect.size.width;
	DMDDimension height = dstRect.size.height;
	DMDDimension x, y;
	
	if (blendMode == DMDBlendModeCopy)
	{
		for (y = 0; y < height; y++)
		{
			DMDColor *srcPtr = DMDFrameGetDotPointer(src, DMDPointMake(srcRect.origin.x, srcRect.origin.y + y));
			DMDColor *dstPtr = DMDFrameGetDotPointer(dst, DMDPointMake(dstRect.origin.x, dstRect.origin.y + y));
			memcpy(dstPtr, srcPtr, width);
		}
	}
	else if(blendMode == DMDBlendModeAdd)
	{
		for (y = 0; y < height; y++)
		{
			DMDColor *srcPtr = DMDFrameGetDotPointer(src, DMDPointMake(srcRect.origin.x, srcRect.origin.y + y));
			DMDColor *dstPtr = DMDFrameGetDotPointer(dst, DMDPointMake(dstRect.origin.x, dstRect.origin.y + y));
			for (x = 0; x < width; x++)
			{
				dstPtr[x] = MIN(dstPtr[x] + srcPtr[x], 0xF);
			}
		}
	}
	else if(blendMode == DMDBlendModeSubtract)
	{
		for (y = 0; y < height; y++)
		{
			DMDColor *srcPtr = DMDFrameGetDotPointer(src, DMDPointMake(srcRect.origin.x, srcRect.origin.y + y));
			DMDColor *dstPtr = DMDFrameGetDotPointer(dst, DMDPointMake(dstRect.origin.x, dstRect.origin.y + y));
			for (x = 0; x < width; x++)
			{
				dstPtr[x] = MAX(dstPtr[x] - srcPtr[x], 0);
			}
		}
	}
	else if(blendMode == DMDBlendModeBlackSource)
	{
		for (y = 0; y < height; y++)
		{
			DMDColor *srcPtr = DMDFrameGetDotPointer(src, DMDPointMake(srcRect.origin.x, srcRect.origin.y + y));
			DMDColor *dstPtr = DMDFrameGetDotPointer(dst, DMDPointMake(dstRect.origin.x, dstRect.origin.y + y));
			for (x = 0; x < width; x++)
			{
				// Only write dots into black dots.
				if ((srcPtr[x] & 0xf) != 0)
					dstPtr[x] = (dstPtr[x] & 0xf0) | (srcPtr[x] & 0xf);
			}
		}
	}
	else if(blendMode == DMDBlendModeAlpha)
	{
		DMDColor *alphaMap = DMDGetAlphaMap();
		
		for (y = 0; y < height; y++)
		{
			DMDColor *srcPtr = DMDFrameGetDotPointer(src, DMDPointMake(srcRect.origin.x, srcRect.origin.y + y));
			DMDColor *dstPtr = DMDFrameGetDotPointer(dst, DMDPointMake(dstRect.origin.x, dstRect.origin.y + y));
			for (x = 0; x < width; x++)
			{
				DMDColor dstValue = dstPtr[x];
				DMDColor srcValue = srcPtr[x];
				
				// Use the alpha map for 'alphaboth', but act as if the dst frame has
				// alpha of 0xf and preserve its original alpha value.
				
				DMDColor v = alphaMap[(unsigned char)srcValue * 256 + (unsigned char)(dstValue | 0xf0)];
				
				dstPtr[x] = (dstValue & 0xf0) | (v & 0x0f);
			}
		}
	}
	else if(blendMode == DMDBlendModeAlphaBoth)
	{
		DMDColor *alphaMap = DMDGetAlphaMap();
		
		for (y = 0; y < height; y++)
		{
			DMDColor *srcPtr = DMDFrameGetDotPointer(src, DMDPointMake(srcRect.origin.x, srcRect.origin.y + y));
			DMDColor *dstPtr = DMDFrameGetDotPointer(dst, DMDPointMake(dstRect.origin.x, dstRect.origin.y + y));
			for (x = 0; x < width; x++)
			{
				char dstValue = dstPtr[x];
				char srcValue = srcPtr[x];
				
				dstPtr[x] = alphaMap[(unsigned char)srcValue * 256 + (unsigned char)dstValue];
			}
		}
	}
}



DMDColor *DMDGetAlphaMap(void)
{
	static DMDColor *gAlphaMap = NULL;
	
	if (gAlphaMap == NULL)
	{
		gAlphaMap = (DMDColor*)malloc(256 * 256);
		//fflush(stderr);
		unsigned src, dst;
		for (src = 0x00; src <= 0xff; src++)
		{
			for (dst = 0x00; dst <= 0xff; dst++)
			{
				char src_dot = (src & 0xf);
				char src_a   = (src >>  4);
				char dst_dot = (dst & 0xf);
				char dst_a   = (dst >>  4);
			
				float i = (float)(src_dot * src_a) / (15.0f * 15.0f);
				float j = (float)(dst_dot * dst_a * (0xf - src_a)) / (15.0f * 15.0f * 15.0f);
			
				char dot = (char)( (i + j) * 15.0f );
				char a   = MAX(src_a, dst_a);
			
				gAlphaMap[src * 256 + dst] = (a << 4) | (dot & 0xf);
				// fprintf(stderr, "%02x -> %02x = %02x\n", src, dst, (unsigned char)gAlphaMap[src * 256 + dst]);
			}
		}
	}
	return gAlphaMap;
}


#define drawdot(subFrame) dots[subFrame*(width*height/8) + ((row*width+col)/8)] |= 1 << (col % 8)

void DMDFrameCopyPROCSubframes(DMDFrame *frame, unsigned char *dots, DMDDimension width, DMDDimension height, unsigned subframes, unsigned char *colorMap)
{
	if (subframes != 4)
	{
		fprintf(stderr, "ERROR in DMDFrameCopyPROCSubframes(): subframes must be 4.");
		return;
	}
	DMDColor defaultColorMap[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
	/* Color map specific to P-ROC: */
	DMDColor procColorMap[] = {0, 2, 8, 10, 1, 3, 9, 11, 4, 6, 12, 14, 5, 7, 13, 15};
	
	/* If the user doesn't specify a color map, use the default. */
	if (!colorMap)
		colorMap = defaultColorMap;

	int row, col;
	
	for (row = 0; row < height; row++)
	{
		for (col = 0; col < width; col++)
		{
			DMDColor dot = DMDFrameGetDot(frame, DMDPointMake(col, row));
			if (dot == 0)
				continue;
			else
			{
				dot &= 0x0f;
				dot = colorMap[(int)dot]; // Apply the mapping from dmd_set_color_mapping()
				dot = procColorMap[(int)dot];
				if (dot & 0x1) drawdot(0);
				if (dot & 0x2) drawdot(1);
				if (dot & 0x4) drawdot(2);
				if (dot & 0x8) drawdot(3);
			}
		}
	}
}


/* DMDRectIntersection() implementation is adapted from Cocotron; license for below lines follows: */

/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

DMDRect DMDRectIntersection(DMDRect rect0, DMDRect rect1)
{
   DMDRect result;

   if(DMDRectGetMaxX(rect0)<=DMDRectGetMinX(rect1) || DMDRectGetMinX(rect0)>=DMDRectGetMaxX(rect1) ||
      DMDRectGetMaxY(rect0)<=DMDRectGetMinY(rect1) || DMDRectGetMinY(rect0)>=DMDRectGetMaxY(rect1))
    return DMDRectMake(0,0,0,0);

   result.origin.x=MAX(DMDRectGetMinX(rect0),DMDRectGetMinX(rect1));
   result.origin.y=MAX(DMDRectGetMinY(rect0),DMDRectGetMinY(rect1));
   result.size.width=MIN(DMDRectGetMaxX(rect0),DMDRectGetMaxX(rect1))-result.origin.x;
   result.size.height=MIN(DMDRectGetMaxY(rect0),DMDRectGetMaxY(rect1))-result.origin.y;

   return result;
}


