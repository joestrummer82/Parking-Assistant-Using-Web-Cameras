/**
 * Parking Assistant Using Web Cameras
 * Martin Kersner's Master Thesis
 *
 * Detection of the closest objects in disparity map.
 *
 * m.kersner@gmail.com
 * 03/27/2015
 */

// TODO specify constants for triangulated input

#include "ObjectDetection.h"

// this constructor is created only in order to posses class without initialization
ObjectDetection::ObjectDetection()
{}

ObjectDetection::ObjectDetection( cv::Mat & _background ) : background(_background)
{}

DisparityObject ObjectDetection::Detect( cv::Mat & scene )
{
    DisparityObject object;
    cv::Mat mask = ((scene - this->background) > this->threshold) / 255.0;


    cv::Mat obstacles = mask.mul(scene);
    cv::Mat hist = ComputeHistogram(obstacles);
    cv::Mat modHist = ModifyHistogram(hist);

    object = ClosestObject(modHist, obstacles);

    return object;
}

cv::Mat ObjectDetection::ComputeHistogram( cv::Mat & src )
{
    cv::Mat hist;
    float range[] = {0, float(this->numberValues)};
    const float* histRange = { range };
    bool uniform    = true; 
    bool accumulate = false;

    cv::calcHist( &src, 1, 0, cv::Mat(), hist, 1, &(this->histSize), &histRange, 
                  uniform, accumulate );


    hist = HistogramPostprocessing(hist);

    return hist;
}

cv::Mat ObjectDetection::HistogramPostprocessing ( cv::Mat & hist ) 
{
    hist.convertTo(hist, CV_16UC1);
    ClearHistogram(hist, 1);

    return hist;
}

cv::Mat ObjectDetection::ModifyHistogram( cv::Mat & hist ) 
{
    cv::Mat modHist = hist.clone();
    cv::Mat posMask;

    modHist -= this->minBase;
    posMask = (modHist > 0) / 255.0;
    posMask.convertTo(posMask, CV_16UC1);

    return hist.mul(posMask);
}

int ObjectDetection::BinId2Intensity( int binId )
{
    return binId * this->binWidth;
}

void ObjectDetection::BinId2BinRange( int binId,
                                      int & lowerBinId,
                                      int & upperBinId )
{
    int tmpLowerBinId = binId - this->leftBinRange;
    int tmpUpperBinId = binId + this->rightBinRange;

    // lower boundary
    if (tmpLowerBinId < 0)
        lowerBinId = 0;
    else
        lowerBinId = tmpLowerBinId;

    // upper boundary
    if (tmpUpperBinId > this->histSize)
        upperBinId = this->histSize - 1;
    else
        upperBinId = tmpUpperBinId;
}

void ObjectDetection::ClearHistogram( cv::Mat & hist,
                                      int offset )
{
    for (int i = this->latestOffset; i < offset; ++i ) // exclusive right boundary
        hist.at<unsigned short>(i) = 0;

    this->latestOffset = offset;
}

/**
 * Computes area of object without spatial information.
 * Computation is based only on boundaries applied to histogram.
 */
int ObjectDetection::AreaOfObject( cv::Mat & hist,
                                   int lowerBin, 
                                   int upperBin )
{
    int area = 0;
    
    for (int i = lowerBin; i <= upperBin; ++i)
        area += hist.at<unsigned short>(i);

    return area;
}

//int ObjectDetection::ClosestObject2( cv::Mat & hist,
//                                    int distance,
//                                    cv::Mat & img ) // only for debug
//{
//    double min, max;
//    int minIdx, maxIdx;
//    int lower, upper;
//    int area;
//    int d;
//
//    cv::minMaxIdx(hist, &min, &max, &minIdx, &maxIdx);
//    BinId2BinRange(maxIdx, lower, upper);
//
//    // TODO delete
//    std::cout << BinId2Intensity(lower) << " " << BinId2Intensity(upper) << std::endl;
//
//    area = AreaOfObject(hist, lower, upper);
//    DisplayObject(img, BinId2Intensity(lower), BinId2Intensity(upper));
//
//    if (area < this->minArea)
//        return distance;
//    else {
//        d = ObjectDistance(hist, lower, upper);
//        ClearHistogram(hist, upper);
//        return ClosestObject2(hist, d, img);
//    }
//
//}

DisparityObject ObjectDetection::ClosestObject ( cv::Mat & hist,
                                                 cv::Mat & obstacles ) // debugging purposes
{
    //int distance;
    int upperBin, lowerBin;
    DisparityObject dispObj;

    // searching for upper boundary
    int i;
    for (i = hist.rows-1; i >= 0; --i) {
        if (hist.at<unsigned short>(i) > 0) {
            upperBin = i+1;
            break;
        }
    }

    // TODO refine
    // not found
    if (i == 0)
        return dispObj;

    // searching for lower boundary
    for (int j = i; j >= 0; --j) {
        if (hist.at<unsigned short>(j) <= 0) {
            lowerBin = j+1;
            break;
        }
    }

    // TODO area control

    // TODO delete
    //std::cout << lowerBin << " " << upperBin << std::endl;
    //DisplayObject(img, BinId2Intensity(lowerBin), BinId2Intensity(upperBin));

    dispObj.found = true;
    dispObj.mask = GetObject( obstacles, 
                              BinId2Intensity(lowerBin), BinId2Intensity(upperBin) );

    return dispObj;
}

float ObjectDetection::ObjectDistance( cv::Mat & hist,
                                       int lowerBin,
                                       int upperBin )
{
    float distance = 0;
    int total      = 0;

    for (int i = lowerBin; i <= upperBin; ++i) { // inclusive right boundary
        distance += hist.at<float>(i) * BinId2Intensity(i);
        total += hist.at<float>(i);
    }

    distance /= total;

    return distance;
}

// AUXILIARY METHODS ///////////////////////////////////////////////////////////
cv::Mat ObjectDetection::GetObject( cv::Mat & src, 
                                 float lower,
                                 float upper )
{
    cv::Mat maskObject;
    cv::Mat maskLower = (src > lower) / 255.0;
    cv::Mat maskUpper = (src < upper) / 255.0;

    cv::Mat mask   = maskLower.mul(maskUpper);
    cv::Mat object = src.mul(mask);

    cv::erode(object, object, cv::Mat(), cv::Point(-1, -1), 4, 1, 1);
    maskObject = (object > 0) / 255.0;

    return maskObject;
}

void ObjectDetection::DisplayHistogram ( cv::Mat & hist )
{
    cv::Mat tmpHist = hist.clone();
    cv::normalize(tmpHist, tmpHist, 0, this->histHeight, cv::NORM_MINMAX, -1, cv::Mat());

    int binWidth = cvRound((double) this->histWidth/this->histSize);
    cv::Mat histImage(this->histHeight, this->histWidth, CV_8UC3, cv::Scalar(0,0,0));

    for (int i = 1; i < this->histSize; i++) {
        line( histImage, 
              cv::Point( binWidth*(i-1), this->histHeight - cvRound(tmpHist.at<float>(i-1)) ),
              cv::Point( binWidth*(i),   this->histHeight - cvRound(tmpHist.at<float>(i)) ),
              cv::Scalar(255, 0, 0), 
              2, 8, 0 );
    }

    cv::imshow("Histogram", histImage);
    cv::waitKey(0);
}
