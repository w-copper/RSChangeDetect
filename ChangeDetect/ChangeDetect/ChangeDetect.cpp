// ChangeDetect.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <math.h>
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "gdal_priv.h"

using namespace cv;

Mat &computeWTH(Mat &src, int d, int s);
Mat &computeDMBP(Mat &src, int d, int s, int ds);
int main()
{
	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	char * pszFilename = "zy-3-wd.img";
	
	/************************************************************************/
	/*   read img                                                           */
	/************************************************************************/
	GDALDataset  *poDataset;
	poDataset = (GDALDataset *)GDALOpen(pszFilename, GA_ReadOnly);
	int Width = poDataset->GetRasterXSize();
	int Height = poDataset->GetRasterYSize();
	int Bands = poDataset->GetRasterCount();
	GDALRasterBand  *poBand;
	//int             nBlockXSize, nBlockYSize;
	//int             bGotMin, bGotMax;
	//double          adfMinMax[2];
	//poBand = poDataset->GetRasterBand(1);
	//poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
	//cout << GDALGetDataTypeName(poBand->GetRasterDataType());
	unsigned short ** pBand = new unsigned short *[Bands];
	//Mat oImg(Height, Width, CV_16UC(3));

	for (int i = 0; i < Bands; i++) {
		pBand[i] = new unsigned short[Width * Height];
		if (pBand[i])
		{
			poBand = poDataset->GetRasterBand(i + 1);
			poBand->RasterIO(GF_Read, 0, 0, Width, Height, pBand[i], Width, Height, GDT_UInt16, 0, 0);
		}
	}
	
	//get light img / anti-water img
	Mat_<unsigned short> predImg(Height, Width);
	//predImg.data = new unsigned short[Width * Height];
	for (int r = 0; r < Height; r++) {
		for (int c = 0; c < Width; c++) {
			uint16_t m = pBand[0][r*Width + c];
			for (int l = 1; l < Bands; l++) {
				int v = pBand[l][r*Width + c];
				if (v > m) {
					m = v;
				}
				predImg(r,c) = m;
			}
		}
	}
	for (int i = 0; i < Bands; i++) {
		delete[] pBand[i];
		pBand[i] = nullptr;
	};
	delete[] pBand;
	pBand = nullptr;
	GDALClose(poDataset);
	//get embi
	const int DARR[11] = {0, 5, 10, 15, 20, 30, 45, 60, 70, 80, 90 };
	const int SARR[4] = { 5, 10, 15, 20 };
	const int DS = 3;
	Mat_<float> embi(Width, Height, 0.0f);

	for (int d = 0; d < 11; d++) {
		for (int s = 0; s < 4; s++) {
			embi += computeDMBP(predImg, d, s, DS);
		}
	}
	embi /= 4 * 11 * 1.0f;

	//thresold
	Mat_<uchar> BinaryImg(Width, Height, (uchar)0);
	float MinMax[2] = { embi(0,0), embi(0,0) };
	for (int r = 0; r < Height; r++) {
		for (int c = 0; c < Width; c++) {
			MinMax[0] = MinMax[0] > embi(r, c) ? embi(r, c) : MinMax[0];
			MinMax[1] = MinMax[1] < embi(r, c) ? embi(r, c) : MinMax[1];
		}
	}
	for (int r = 0; r < Height; r++) {
		for (int c = 0; c < Width; c++) {
			BinaryImg(r, c) = uchar((embi(r, c) - MinMax[0]) / (MinMax[1] - MinMax[0]) * 255);
		}
	}
	adaptiveThreshold(BinaryImg, BinaryImg, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 7, 1);

	//post processing
	//img save and show and relase
	imwrite("../embi.tif", embi);
	namedWindow("embi");
	imshow("embi", embi);
	embi.release();
	imwrite("../binary.tif", BinaryImg);
	namedWindow("binary");
	imshow("binary", BinaryImg);
	BinaryImg.release();

    return 0;
}

Mat &createStrictLineElement(int d, int s) {
	assert(d >= 0 && d <= 45);
	assert(s >= 2);
	int width = int(s * cos(d * 1.0) + 0.5);
	int height = int(s * sin(d*1.0) + 0.5 );
	Mat_<uchar> e(width, height, (uchar)0);
	double k = tan(d);
	for (int i = 0; i < width; i++) {
		int j = int(round(k * i));
		if (j >= height) j = height;
		e(i, j) = 1;
	}
	return e;
}

Mat &createLineElement(int d, int s) {
	if (s <= 1) {
		return Mat_<uchar>(1, 1, (uchar)1);
	}
	while (d >= 180) {
		d -= 180;
	}

	if (d <= 45) {
		return createStrictLineElement(d, s);
	}
	else if (d <= 90) {
		Mat e = createStrictLineElement(d - 45, s).t();
		return e;
	}
	else if (d <= 135) {
		Mat e = createStrictLineElement(180 - d, s).t();
		flip(e, e, 0);
		return e;
	}
	else {
		Mat e = createStrictLineElement(180 - d, s);
		flip(e, e, 0);
		return e;
	}
}

Mat &D_g(Mat &src, int n, Mat &b, Mat &g) {
	if (n == 0) {
		return src;
	}
	if (n == 1) {
		//cv::min()
	}
}

Mat &computeWTH(Mat &src, int d, int s) {
//http://www.pianshen.com/article/388421804/
}
Mat &computeDMBP(Mat &src, int d, int s, int ds) {

}