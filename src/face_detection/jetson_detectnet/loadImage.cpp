/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// Kookmin University
// School of Computer Science
// Capstone #4 Flex Ads
// 20132651 Lee Sungjae

#include "loadImage.h"
#include "cudaMappedMemory.h"
#include "filesystem.h"

#include <QImage>

// Bounding Box 에 표시된 (x_min, y_min), (x_max, y_max) 좌표를 이용하여
// 입력된 이미지를 Cropping 하여 새로운 QImage 객체에 Copy 하고 저장하는 함수이다.
bool saveCropImageRGBA( const char* filename, float4* cpu, int y_min, int y_max, int x_min, int x_max, int width, int height, float max_pixel )
{
	// Bounding Box 로 지정된 부분의 이미지 너비와 높이를 width_crop 과 height_crop 변수에 저장한다.
	int width_bb = x_max - x_min;
	int height_bb = y_max - y_min;

	//Bounding Box 보다 큰 사이즈의 Cropping 영역을 새롭게 정의해야 한다.
	// 왼쪽, 오른쪽, 하단은 너비와 높이의 0.6배, 상단은 높이의 0.3배 확장한 이미지를 Cropping 한다.
	x_min = x_min - (width_bb * 0.6);
	x_max = x_max + (width_bb * 0.6);
	y_min = y_min - (height_bb * 0.3);
	y_max = y_max + (height_bb * 0.6);

	const float scale = 255.0f / max_pixel;

	// Bounding Box 보다 큰 사이즈의 Cropping 영역을 이미지 객체로 생성한다.
	// 앞에서 재정의한 x max, min 과 y max, min 을 기준으로 width 와 height 를 계산한다.
	QImage img(x_max - x_min, y_max - y_min, QImage::Format_RGB32);

    // 2중 for 문을 통해 원본 이미지의 Cropping 할 부분에 해당하는 픽셀 데이터에 접근한다.
	for( int y=y_min; y < y_max; y++ )
	{
		for( int x=x_min; x < x_max; x++ )
		{
            // 해당 픽셀 데이터를 px 로 저장하며, 이를 img 객체에 setPixel 을 이용하여 copy 한다.
            // 이 때, 좌표계가 (0,0) 부터 (width_crop, height_crop) 까지 작성되야 하므로
            // 시작점인 y_min, x_min 만큼을 x, y 값에서 뺀 위치에 저장하게 된다.
			const float4 px = cpu[y*width + x];
			img.setPixel(x-x_min, y-y_min, qRgb(px.x * scale, px.y * scale, px.z * scale));
		}
	}

    // 입력받은 filename 으로 이미지를 저장한다.
	if( !img.save(filename))
	{
		printf("failed to save %ix%i output image to %s\n", x_max - x_min, y_max - y_min, filename);
		return false;
	}

	return true;
}


bool saveImageRGBA( const char* filename, float4* cpu, int width, int height, float max_pixel )
{
	if( !filename || !cpu || !width || !height )
	{
		printf("saveImageRGBA - invalid parameter\n");
		return false;
	}

	const float scale = 255.0f / max_pixel;
	QImage img(width, height, QImage::Format_RGB32);

	for( int y=0; y < height; y++ )
	{
		for( int x=0; x < width; x++ )
		{
			const float4 px = cpu[y * width + x];
			//printf("%03u %03u   %f\n", x, y, normPx);
			img.setPixel(x, y, qRgb(px.x * scale, px.y * scale, px.z * scale));
		}
	}


	/*
	 * save file
	 */
	if( !img.save(filename/*, "PNG", 100*/) )
	{
		printf("failed to save %ix%i output image to %s\n", width, height, filename);
		return false;
	}

	return true;
}


// loadImageRGBA
bool loadImageRGBA( const char* filename, float4** cpu, float4** gpu, int* width, int* height )
{
	if( !filename || !cpu || !gpu || !width || !height )
	{
		printf("loadImageRGBA - invalid parameter\n");
		return false;
	}

	// verify file path
	const std::string path = locateFile(filename);

	if( path.length() == 0 )
	{
		printf("failed to find image '%s'\n", filename);
		return false;
	}

	// load original image
	QImage qImg;

	if( !qImg.load(path.c_str()) )
	{
		printf("failed to load image '%s'\n", path.c_str());
		return false;
	}

	if( *width != 0 && *height != 0 )
		qImg = qImg.scaled(*width, *height, Qt::IgnoreAspectRatio);

	const uint32_t imgWidth  = qImg.width();
	const uint32_t imgHeight = qImg.height();
	const uint32_t imgPixels = imgWidth * imgHeight;
	const size_t   imgSize   = imgWidth * imgHeight * sizeof(float) * 4;

	printf("loaded image  %s  (%u x %u)  %zu bytes\n", filename, imgWidth, imgHeight, imgSize);

	// allocate buffer for the image
	if( !cudaAllocMapped((void**)cpu, (void**)gpu, imgSize) )
	{
		printf(LOG_CUDA "failed to allocated %zu bytes for image %s\n", imgSize, filename);
		return false;
	}

	float4* cpuPtr = *cpu;

	for( uint32_t y=0; y < imgHeight; y++ )
	{
		for( uint32_t x=0; x < imgWidth; x++ )
		{
			const QRgb rgb  = qImg.pixel(x,y);
			const float4 px = make_float4(float(qRed(rgb)),
										  float(qGreen(rgb)),
										  float(qBlue(rgb)),
										  float(qAlpha(rgb)));

			cpuPtr[y*imgWidth+x] = px;
		}
	}

	*width  = imgWidth;
	*height = imgHeight;
	return true;
}


// loadImageRGB
bool loadImageRGB( const char* filename, float3** cpu, float3** gpu, int* width, int* height, const float3& mean )
{
	if( !filename || !cpu || !gpu || !width || !height )
	{
		printf("loadImageRGB - invalid parameter\n");
		return false;
	}

	// verify file path
	const std::string path = locateFile(filename);

	if( path.length() == 0 )
	{
		printf("failed to find image '%s'\n", filename);
		return false;
	}

	// load original image
	QImage qImg;

	if( !qImg.load(path.c_str()) )
	{
		printf("failed to load image '%s'\n", path.c_str());
		return false;
	}

	if( *width != 0 && *height != 0 )
		qImg = qImg.scaled(*width, *height, Qt::IgnoreAspectRatio);

	const uint32_t imgWidth  = qImg.width();
	const uint32_t imgHeight = qImg.height();
	const uint32_t imgPixels = imgWidth * imgHeight;
	const size_t   imgSize   = imgWidth * imgHeight * sizeof(float) * 3;

	printf("loaded image  %s  (%u x %u)  %zu bytes\n", filename, imgWidth, imgHeight, imgSize);

	// allocate buffer for the image
	if( !cudaAllocMapped((void**)cpu, (void**)gpu, imgSize) )
	{
		printf(LOG_CUDA "failed to allocated %zu bytes for image %s\n", imgSize, filename);
		return false;
	}

	float* cpuPtr = (float*)*cpu;

	for( uint32_t y=0; y < imgHeight; y++ )
	{
		for( uint32_t x=0; x < imgWidth; x++ )
		{
			const QRgb rgb  = qImg.pixel(x,y);
			const float mul = 1.0f; 	//1.0f / 255.0f;
			const float3 px = make_float3((float(qRed(rgb))   - mean.x) * mul,
										  (float(qGreen(rgb)) - mean.y) * mul,
										  (float(qBlue(rgb))  - mean.z) * mul );

			// note:  caffe/GIE is band-sequential (as opposed to the typical Band Interleaved by Pixel)
			cpuPtr[imgPixels * 0 + y * imgWidth + x] = px.x;
			cpuPtr[imgPixels * 1 + y * imgWidth + x] = px.y;
			cpuPtr[imgPixels * 2 + y * imgWidth + x] = px.z;
		}
	}

	*width  = imgWidth;
	*height = imgHeight;
	return true;
}


// loadImageBGR
bool loadImageBGR( const char* filename, float3** cpu, float3** gpu, int* width, int* height, const float3& mean )
{
	if( !filename || !cpu || !gpu || !width || !height )
	{
		printf("loadImageRGB - invalid parameter\n");
		return false;
	}

	// verify file path
	const std::string path = locateFile(filename);

	if( path.length() == 0 )
	{
		printf("failed to find image '%s'\n", filename);
		return false;
	}

	// load original image
	QImage qImg;

	if( !qImg.load(path.c_str()) )
	{
		printf("failed to load image '%s'\n", path.c_str());
		return false;
	}

	if( *width != 0 && *height != 0 )
		qImg = qImg.scaled(*width, *height, Qt::IgnoreAspectRatio);

	const uint32_t imgWidth  = qImg.width();
	const uint32_t imgHeight = qImg.height();
	const uint32_t imgPixels = imgWidth * imgHeight;
	const size_t   imgSize   = imgWidth * imgHeight * sizeof(float) * 3;

	printf("loaded image  %s  (%u x %u)  %zu bytes\n", filename, imgWidth, imgHeight, imgSize);

	// allocate buffer for the image
	if( !cudaAllocMapped((void**)cpu, (void**)gpu, imgSize) )
	{
		printf(LOG_CUDA "failed to allocated %zu bytes for image %s\n", imgSize, filename);
		return false;
	}

	float* cpuPtr = (float*)*cpu;

	for( uint32_t y=0; y < imgHeight; y++ )
	{
		for( uint32_t x=0; x < imgWidth; x++ )
		{
			const QRgb rgb  = qImg.pixel(x,y);
			const float mul = 1.0f; 	//1.0f / 255.0f;
			const float3 px = make_float3((float(qBlue(rgb))  - mean.x) * mul,
										  (float(qGreen(rgb)) - mean.y) * mul,
										  (float(qRed(rgb))   - mean.z) * mul );

			// note:  caffe/GIE is band-sequential (as opposed to the typical Band Interleaved by Pixel)
			cpuPtr[imgPixels * 0 + y * imgWidth + x] = px.x;
			cpuPtr[imgPixels * 1 + y * imgWidth + x] = px.y;
			cpuPtr[imgPixels * 2 + y * imgWidth + x] = px.z;
		}
	}

	return true;
}
