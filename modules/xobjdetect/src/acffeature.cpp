/*

By downloading, copying, installing or using the software you agree to this
license. If you do not agree to this license, do not download, install,
copy or use the software.


                          License Agreement
               For Open Source Computer Vision Library
                       (3-clause BSD License)

Copyright (C) 2013, OpenCV Foundation, all rights reserved.
Third party copyrights are property of their respective owners.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the names of the copyright holders nor the names of the contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

This software is provided by the copyright holders and contributors "as is" and
any express or implied warranties, including, but not limited to, the implied
warranties of merchantability and fitness for a particular purpose are
disclaimed. In no event shall copyright holders or contributors be liable for
any direct, indirect, incidental, special, exemplary, or consequential damages
(including, but not limited to, procurement of substitute goods or services;
loss of use, data, or profits; or business interruption) however caused
and on any theory of liability, whether in contract, strict liability,
or tort (including negligence or otherwise) arising in any way out of
the use of this software, even if advised of the possibility of such damage.

*/

#include "precomp.hpp"

using std::vector;

using std::min;

#include <iostream>
using std::cout;
using std::endl;

namespace cv
{
namespace xobjdetect
{

class ACFFeatureEvaluatorImpl : public ACFFeatureEvaluator
{
public:
    ACFFeatureEvaluatorImpl(const vector<Point3i>& features):
        features_(features), channels_(), position_()
    {
        CV_Assert(features.size() > 0);
    }

    virtual void setChannels(InputArrayOfArrays channels);
    virtual void setPosition(Size position);
    virtual int evaluate(size_t feature_ind) const;
    virtual void evaluateAll(OutputArray feature_values) const;

private:
    /* Features to evaluate */
    std::vector<Point3i> features_;
    /* Channels for feature evaluation */
    std::vector<Mat> channels_;
    /* Channels window position */
    Size position_;
};

void ACFFeatureEvaluatorImpl::setChannels(cv::InputArrayOfArrays channels)
{
    channels_.clear();
    vector<Mat> ch;
    channels.getMatVector(ch);
    CV_Assert(ch.size() == 10);
    for( size_t i = 0; i < ch.size(); ++i )
    {
        const Mat &channel = ch[i];
        Mat_<int> acf_channel(channel.rows / 4, channel.cols / 4);
        for( int row = 0; row < channel.rows; row += 4 )
        {
            for( int col = 0; col < channel.cols; col += 4 )
            {
                int sum = 0;
                for( int cell_row = row; cell_row < row + 4; ++cell_row )
                    for( int cell_col = col; cell_col < col + 4; ++cell_col )
                        sum += (int)channel.at<float>(cell_row, cell_col);

                acf_channel(row / 4, col / 4) = sum;
            }
        }
        channels_.push_back(acf_channel);
    }
}

void ACFFeatureEvaluatorImpl::setPosition(Size position)
{
    position_ = position;
}

int ACFFeatureEvaluatorImpl::evaluate(size_t feature_ind) const
{
    CV_Assert(channels_.size() == 10);
    CV_Assert(feature_ind < features_.size());

    Point3i feature = features_.at(feature_ind);
    int x = feature.x;
    int y = feature.y;
    int n = feature.z;
    return channels_[n].at<int>(y, x);
}

void ACFFeatureEvaluatorImpl::evaluateAll(OutputArray feature_values) const
{
    Mat_<int> feature_vals(1, (int)features_.size());
    for( int i = 0; i < (int)features_.size(); ++i )
    {
        feature_vals(0, i) = evaluate(i);
    }
    feature_values.setTo(feature_vals);
}

Ptr<ACFFeatureEvaluator>
createACFFeatureEvaluator(const vector<Point3i>& features)
{
    return Ptr<ACFFeatureEvaluator>(new ACFFeatureEvaluatorImpl(features));
}

vector<Point3i> generateFeatures(Size window_size, int count)
{
    CV_Assert(count > 0);
    int cur_count = 0;
    int max_count = window_size.width * window_size.height / 16;
    count = min(count, max_count);
    vector<Point3i> features;
    for( int x = 0; x < window_size.width / 4; ++x )
    {
        for( int y = 0; y < window_size.height / 4; ++y )
        {
            /* Assume there are 10 channel types */
            for( int n = 0; n < 10; ++n )
            {
                features.push_back(Point3i(x, y, n));
                if( (cur_count += 1) == count )
                    break;
            }
        }
    }
    return features;
}

void computeChannels(cv::InputArray image, cv::OutputArrayOfArrays channels_)
{
    Mat src(image.getMat().rows, image.getMat().cols, CV_32FC3);
    image.getMat().convertTo(src, CV_32FC3, 1./255);

    Mat_<float> grad;
    Mat gray;
    cvtColor(src, gray, CV_RGB2GRAY);

    Mat_<float> row_der, col_der;
    Sobel(gray, row_der, CV_32F, 0, 1);
    Sobel(gray, col_der, CV_32F, 1, 0);

    magnitude(row_der, col_der, grad);

    Mat_<Vec6f> hist(grad.rows, grad.cols);
    const float to_deg = 180 / 3.1415926f;
    for (int row = 0; row < grad.rows; ++row) {
        for (int col = 0; col < grad.cols; ++col) {
            float angle = atan2(row_der(row, col), col_der(row, col)) * to_deg;
            if (angle < 0)
                angle += 180;
            int ind = (int)(angle / 30);
            hist(row, col)[ind] = grad(row, col);
        }
    }

    vector<Mat> channels;
    channels.push_back(gray);
    channels.push_back(grad);

    vector<Mat> hist_channels;
    split(hist, hist_channels);

    for( size_t i = 0; i < hist_channels.size(); ++i )
        channels.push_back(hist_channels[i]);

    channels_.setTo(channels);
}

} /* namespace xobjdetect */
} /* namespace cv */
