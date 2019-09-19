// ChangeDetect.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <math.h>
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "gdal_priv.h"

#define  PI 3.1415926
#define LOG 1
#define D2R(d) (d) * PI / 180.0
using namespace cv;

void computeWTH(Mat &src, int d, int s, Mat&);
void computeDMBP(Mat &src, int d, int s, int ds, Mat&);
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
	if (LOG) {
		cout << "打开图像" <<endl;
	}
	int Width =  poDataset->GetRasterXSize();
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
	if (LOG) {
		cout << "读取图像" << endl;
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
	if (LOG) {
		cout << "计算亮度" << endl;
	}
	//get embi
	const int DARR[11] = {0, 5, 10, 15, 20, 30, 45, 60, 70, 80, 90 };
	const int SARR[4] = { 5, 10, 15, 20 };
	const int DS = 3;
	Mat_<float> embi(Height, Width, 0.0f), dmp(Height, Width, 0.0f);
	embi += dmp;
	for (int d = 0; d < 11; d++) {
		for (int s = 0; s < 4; s++) {
			computeDMBP(predImg, DARR[d], SARR[s], DS, dmp);
			if (LOG) {
				cout << "s" << SARR[s] << "," << "d" << DARR[d]<< endl;
			}
			embi += dmp;
		}
	}
	
	embi *= 1 / (4 * 11 * 1.0f);
	if (LOG) {
		cout << "计算EMBI" << endl;
	}
	//thresold
	Mat_<uchar> BinaryImg(Height, Width, (uchar)0);
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
	if (LOG) {
		cout << "阈值处理" << endl;
	}
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
	waitKey(0);

    return 0;
}

void createStrictLineElement(int d, int s, Mat &e ) {
	assert(d >= 0 && d <= 45);
	assert(s >= 2);
	int width = int(s * cos(D2R(d*1.0)) + 0.5) +1;
	int height = int(s * sin(D2R(d*1.0)) + 0.5 ) +1;
	Mat_<uchar> ee(width, height, (uchar)0);
	double k = tan(D2R(d));
	for (int i = 0; i < width; i++) {
		int j = int(round(k * i));
		if (j >= height) j = height -1;
		ee(i, j) = 1;
	}
	e = ee;
}

void createLineElement(int d, int s, Mat &e) {
	if (s <= 1) {
		e = Mat_<uchar>(1, 1, (uchar)1);
	}
	while (d >= 180) {
		d -= 180;
	}

	if (d <= 45) {
		createStrictLineElement(d, s, e);
	}
	else if (d <= 90) {
		createStrictLineElement(d - 45, s, e);
		e = e.t();
	}
	else if (d <= 135) {
		createStrictLineElement(180 - d, s, e);
		e = e.t();
		flip(e, e, 0);
	}
	else {
		createStrictLineElement(180 - d, s, e);
		flip(e, e, 0);
	}
}

void D_g(Mat &src, int n, Mat &kernel, Mat &ground, Mat &d) {
	if (n == 0) {
		d = src;
		return;
	}
	if (n == 1) {
		dilate(src, d, kernel);
		d = cv::min(d, ground);
		return;
	}
	Mat dd;
	D_g(src, n - 1, kernel, ground, dd);
	D_g(dd, 1, kernel, ground, d);
}

void E_g(Mat &src, int n, Mat &kernel, Mat &ground, Mat &d) {
	if (n == 0) {
		d = src;
	}
	if (n == 1) {
		erode(src, d, kernel);
		d = cv::max(d, ground);
		return;
	}
	Mat dd;
	E_g(src, n - 1, kernel, ground, dd);
	E_g(dd, 1, kernel, ground, d);
	//return E_g(E_g(src, n - 1, b, g), 1, b, g);
}

void OpenReconstruct(Mat &src, Mat&ekernel, Mat&gkernel, Mat&d) {
	Mat newImg;
	Mat f;
	erode(src, f, ekernel);
	while (true)
	{
		D_g(f, 1, gkernel, src, newImg);
		Mat diff;
		absdiff(newImg, f, diff);
		if (sum(diff)(0) <= 1e-3) {
			d = newImg;
			return;
			break;
		}
		f = newImg;
	}
	d = src;
}

void CloseReconstruct(Mat &src, Mat&dkernel, Mat&gkernel, Mat&d) {
	Mat newImg;
	Mat f;
	dilate(src, f, dkernel);
	while (true)
	{
		E_g(f, 1, gkernel, src, newImg);
		Mat diff;
		absdiff(newImg, f, diff);
		if (sum(diff)(0) <= 1e-3) {
			d = newImg;
			return;
			break;
		}
		f = newImg;
	}
	d = src;
}

void computeWTH(Mat &src, int d, int s, Mat &dst) {
	Mat e;
	createLineElement(d, s, e);
	Mat closeR;
	CloseReconstruct(src, e, e, closeR);
	Mat openR;
	OpenReconstruct(closeR, e, e, openR);
	dst = closeR - openR;
	return;
}
void computeDMBP(Mat &src, int d, int s, int ds, Mat &dst) {
	Mat mp1;
	computeWTH(src, d, s, mp1);
	Mat mp2;
	computeWTH(src, d, s + ds, mp2);
	Mat dd;
	absdiff(mp1, mp2, dd);
	dd.convertTo(dst, CV_32F);
}