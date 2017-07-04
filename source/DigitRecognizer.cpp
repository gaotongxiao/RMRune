#include <iostream>
#include <string>
#include <stdexcept> 
#include "DigitRecognizer.h"
#include "RuneDetector.hpp"
#define SHOW_IMAGE
#define ADJUST_HSV

void binaryMat2points(const Mat & img, vector<Point> & pts)
{
	for(int x = 0; x < img.cols; x++)
	{
		for(int y = 0; y < img.rows; y++)
		{
			if(img.at<char>(x, y) >0)
			{
				pts.push_back(Point(x, y));
			}
		}
	}

}


DigitRecognizer::DigitRecognizer(Settings::LightSetting lightSetting)
{

	rng = RNG(12345);

    lowerBound = lightSetting.hsvLowerBound;
    upperBound = lightSetting.hsvUpperBound;

    segmentTable = {
    	{119, 0},
    	{18, 1},
    	{93, 2},
    	{91, 3},
    	{58, 4},
    	{107, 5},
    	{111, 6},
    	{82, 7},
    	{127, 8},
    	{123, 9}
    };

    segmentRects = {
    	Rect(Point(10, 0), Point(35, 5)), // 0
    	Rect(Point(6, 5), Point(11, 27)), // 1
    	Rect(Point(32, 5), Point(37, 27)), //2
	    Rect(Point(5, 27), Point(35, 32)), // 3
	    Rect(Point(3, 32), Point(8, 58)), // 4
	    Rect(Point(27, 32), Point(32, 58)), //5
	    Rect(Point(5, 53), Point(30, 58)) // 6
    };

}

void DigitRecognizer::predict(const Mat& inputImg, const Rect2f & sudokuPanel)
{
	clear();

	Rect2f digitBoardRect = sudokuPanel;
	digitBoardRect.width = sudokuPanel.width / (280.0*3)*104.5*5;
	digitBoardRect.height = sudokuPanel.height / (160.0*3)*125.0;
	digitBoardRect -= Point2f(sudokuPanel.width * 0.25, sudokuPanel.height * 0.8);
	Mat img = inputImg(digitBoardRect);
	Mat grayImg;
	Mat digitBoardImg = img;
	cvtColor(img, grayImg, CV_BGR2GRAY);
	GaussianBlur(grayImg, grayImg, Size(9, 9), 0);
	Canny(grayImg, grayImg, 120, 120*2, 3);
#ifdef SHOW_IMAGE
	imshow("Canny", grayImg);
#endif


	vector<vector<Point> > digitContours;
	vector<Vec4i> digitHierarchy;
	findContours( grayImg, digitContours, digitHierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );

	vector<vector<Point> > digitContoursPolys;
	vector<Rect2f> digitBoundRects;
	vector<Rect2f> oneBoundRects;
	int digitAvgWidth = 0;
	float lowerThreshold = 1;
	float upperThreshold = 1.6;
	for ( int i = 0; i < digitContours.size(); i++ ) 
	{
		vector<Point> curDigitContoursPoly;
		approxPolyDP( digitContours.at(i), curDigitContoursPoly, 3, true );
		digitContoursPolys.push_back(curDigitContoursPoly);
		Rect2f curBoundingRect = boundingRect(Mat(curDigitContoursPoly));
		float ratio = (float) curBoundingRect.width / (float) curBoundingRect.height;	
		if (ratio < 0.5 * upperThreshold && ratio > 0.5 *lowerThreshold)
		{
			digitAvgWidth += curBoundingRect.width;
			digitBoundRects.push_back(curBoundingRect);
		}
		else if ( ratio < 0.32 && ratio > 0.25)
		{
			cout << "One Ratio: " << ratio << endl;
			oneBoundRects.push_back(curBoundingRect);
		}
	}
	digitAvgWidth /= digitBoundRects.size();
	if (digitBoundRects.size() != 5)
	{
		if (digitBoundRects.size() == 4 && oneBoundRects.size() == 1)
		{
			Rect2f curBoundingRect = oneBoundRects.at(0);
			curBoundingRect.width = digitAvgWidth;
			curBoundingRect -= Point2f(0.6 * digitAvgWidth, 0);
			digitBoundRects.push_back(curBoundingRect);
		}
		else return;
	}
	// for (int i = 0; i < digitBoundRects.size(); i++) {
	// 	rectangle( digitBoardImg, digitBoundRects.at(i), Scalar(255, 255, 255));
	// }
	// imshow("digitBoardRect", digitBoardImg);
	// waitKey(0);
	sort(digitBoundRects.begin(), digitBoundRects.end(), [] (Rect a, Rect b) { return a.x < b.x; });
	// digitBoardRect = Rect2f(digitBoundRects.at(0).tl(), digitBoundRects.at(digitBoundRects.size()-1).br());
	// int widthGap = (digitBoardRect.width - digitAvgWidth)/4;
	// digitAvgWidth /= digitBoundRects.size();
	// digitBoundRects.clear();
	// for (int i = 0; i < 5; ++i)
	// {
	// 	Rect2f curRect = Rect2f(0, 0, digitAvgWidth, digitBoardRect.height);
	// 	curRect = curRect + Point2f(i * (digitAvgWidth + widthGap), 0);
	// 	digitBoundRects.push_back(curRect);
	// }
	// Mat digitBoardImg = img(digitBoardRect);
	// for (int i = 0; i < digitBoundRects.size(); i++) {
	// 	rectangle( digitBoardImg, digitBoundRects.at(i), Scalar(255, 255, 255));
	// }
	// imshow("digitBoardRect", digitBoardImg);

    Mat hsvFrame;
    cvtColor(digitBoardImg, hsvFrame, CV_BGR2HSV);
    inRange(hsvFrame, lowerBound, upperBound, hsvFrame);
    hsvFrame.copyTo(digitBoardImg);
#ifdef SHOW_IMAGE
    imshow("hsvFrame", hsvFrame);
#endif

	for (int i = 0; i < digitBoundRects.size(); ++i)
	{
		Mat curImg = digitBoardImg(digitBoundRects.at(i));
		resize(curImg, curImg, Size(40, 60));
		digitImgs.push_back(curImg);
	}
	cout << "Digit: " << digitImgs.size() << endl;
	for (int i = 0; i < digitImgs.size(); ++i)
	{
		digitLabels.push_back(recognize(digitImgs.at(i)));
		cout << digitLabels.at(i);
	}
	cout << endl;
	
}

#ifdef ADJUST_HSV
struct DigitRecognizerUserData {
	Scalar lowerBound;
	Scalar upperBound;
	Mat hsvImg;
};

void AdjustHSVImg(int v, void* d)
{
	DigitRecognizerUserData* data = (DigitRecognizerUserData*)d;
	Scalar testLowerBound = data->lowerBound;
	Mat resImg;
	testLowerBound[2] = v;
	inRange(data->hsvImg, testLowerBound, data->upperBound, resImg);
	imshow("AdjustHsvImg", resImg);
}
#endif

int DigitRecognizer::process(const Mat& img)
{
	Mat hsvImg;
	GaussianBlur(img, hsvImg, Size(9, 9), 0);
	dilate(hsvImg, hsvImg, getStructuringElement(0, Size(9, 9)));
	cvtColor(img, hsvImg, CV_BGR2HSV);
#ifdef ADJUST_HSV
	int V = lowerBound[2];
	DigitRecognizerUserData data;
	data.lowerBound = lowerBound;
	data.upperBound = upperBound;
	hsvImg.copyTo(data.hsvImg);
	namedWindow("AdjustHsvImg", WINDOW_NORMAL);
	createTrackbar("Adjust Hsv", "AdjustHsvImg", &V, 255, AdjustHSVImg, (void*)&data);
	AdjustHSVImg(lowerBound[2], (void*)&data);
	waitKey(0);
	return 0;
#else
	inRange(hsvImg, lowerBound, upperBound, hsvImg);
#endif
	Mat hsvCopy;
	hsvImg.copyTo(hsvCopy);
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	findContours( hsvCopy, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );			
	sort(contours.begin(), contours.end(), [](const vector<Point> & a, const vector<Point> & b) {return a.size()>b.size();});
	vector<Point> curContoursPoly;
	approxPolyDP(contours.at(0), curContoursPoly, 3, true);
	Rect curBoundingRect = boundingRect(curContoursPoly);
	float ratio = (float)curBoundingRect.width / (float)curBoundingRect.height;
	int ret = 0;
	if (ratio < 0.5)
	{
		resize(hsvImg, hsvImg, Size(40, 60));
		ret = recognize(hsvImg);
	}
	else{
		hsvImg = hsvImg(boundingRect(curContoursPoly));
		resize(hsvImg, hsvImg, Size(40, 60));
		ret = recognize(hsvImg);
	} 
#ifdef SHOW_IMAGE
	imshow("hsvImg", hsvImg);
#endif
	return ret;
}

DigitRecognizer::~DigitRecognizer()
{
	digitImgs.clear();
	digitLabels.clear();
}


int DigitRecognizer::recognize(const Mat& img)
{
	int ret = 0;
	Mat temp;
	img.copyTo(temp);


	for (int i = 0; i < segmentRects.size(); ++i)
	{
		ret <<= 1;
		Mat curImg = img(segmentRects.at(i));
		int total = countNonZero(curImg);
		if ((float)total/ (float) segmentRects.at(i).area() > 0.5)
		{
			ret += 1;
		}
		rectangle(temp, segmentRects.at(i), Scalar(255, 255, 255));
	}
	try
	{
		ret = segmentTable.at(ret);
	}
	catch (out_of_range e)
	{

		return -1;
	}
#ifdef SHOW_IMAGE
	imshow("temp", temp);
#endif
	return ret;
}


void DigitRecognizer::clear()
{
	digitImgs.clear();
}
