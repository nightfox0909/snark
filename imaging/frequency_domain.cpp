// This file is part of snark, a generic and flexible library
// for robotics research.
//
// Copyright (C) 2011 The University of Sydney
//
// snark is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// snark is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
// for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with snark. If not, see <http://www.gnu.org/licenses/>.

#include <snark/imaging/frequency_domain.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>

namespace snark{ namespace imaging {

/// constructor
/// @param mask window to be applied in the frequencty
frequency_domain::frequency_domain( const cv::Mat& mask ):
    m_mask( mask )
{
    shift( m_mask );
}

/// transform image into frequency domain, apply window and transform back to space domain
cv::Mat frequency_domain::filter( const cv::Mat& image, const cv::Mat& mask )
{
    double maxValue;
    cv::minMaxIdx( image, NULL, &maxValue );
    cv::Mat padded;
    cv::Mat paddedMask;                      //expand input image to optimal size
    int m = cv::getOptimalDFTSize( image.rows );
    int n = cv::getOptimalDFTSize( image.cols ); // on the border add zero values
    cv::copyMakeBorder( image, padded, 0, m - image.rows, 0, n - image.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0) );
    cv::copyMakeBorder( m_mask, paddedMask, 0, m - m_mask.rows, 0, n - m_mask.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0) );

    cv::Mat planes[] = { cv::Mat_<float>(padded), cv::Mat::zeros(padded.size(), CV_32F) };
    cv::merge( planes, 2, m_complexImage );         // Add to the expanded another plane with zeros
    cv::dft( m_complexImage, m_complexImage );            // this way the result may fit in the source matrix
    
    cv::Mat masked;
    m_complexImage.copyTo( masked, paddedMask );
    
    cv::Mat inverse;
    cv::dft( masked, inverse, cv::DFT_INVERSE | cv::DFT_REAL_OUTPUT );
    cv::Mat abs = cv::abs( inverse );
    if( mask.rows != 0 )
    {
        cv::Mat maskF;// = cv::Mat_<float>( mask );
        cv::Mat paddedMaskF;
        mask.convertTo( maskF, CV_32F, 1.0/255.0 );
        if( abs.cols > maskF.cols || abs.rows > maskF.rows )
        {
            cv::copyMakeBorder( maskF, paddedMaskF, 0, abs.rows - maskF.rows, 0, abs.cols - maskF.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0) );
        }
        else
        {
            paddedMaskF = maskF;
        }
        cv::multiply( abs, paddedMaskF, abs );
    }
    cv::Mat result;
    cv::normalize( abs, abs, 0, maxValue, CV_MINMAX ); // scale to get the same max value as the original TODO better scaling ?
    abs.convertTo( result, CV_8U );
    return result;
}

/// get the magnitude of the image in frequency domain
cv::Mat frequency_domain::magnitude() const
{    
    // compute the magnitude and switch to logarithmic scale
    // => log(1 + sqrt(Re(DFT(I))^2 + Im(DFT(I))^2))
    cv::Mat planes[2];
    cv::split( m_complexImage, planes );                   // planes[0] = Re(DFT(I), planes[1] = Im(DFT(I))
    cv::magnitude( planes[0], planes[1], planes[0] );// planes[0] = magnitude
    cv::Mat magnitudeImage = planes[0];

    magnitudeImage += cv::Scalar::all(1);                    // switch to logarithmic scale
    cv::log( magnitudeImage, magnitudeImage );

    // crop the spectrum, if it has an odd number of rows or columns
    magnitudeImage = magnitudeImage( cv::Rect( 0, 0, magnitudeImage.cols & -2, magnitudeImage.rows & -2 ) );
    shift( magnitudeImage );
    
 // transform the matrix with float values into a viewable image form (float between values 0 and 1).
    cv::normalize( magnitudeImage, magnitudeImage, 0, 1, CV_MINMAX );
    return magnitudeImage;
}

/// rearrange the quadrants of Fourier image  so that the origin is at the image center
void frequency_domain::shift( cv::Mat& image )
{
    if( image.rows == 1 && image.cols == 1)
    {
        return;
    }

    cv::vector< cv::Mat > planes;
    cv::split( image, planes );

    int xMid = image.cols >> 1;
    int yMid = image.rows >> 1;

    for( unsigned int i = 0; i < planes.size(); i++ )
    {
        cv::Mat tmp;
        cv::Mat q0(planes[i], cv::Rect(0,    0,    xMid, yMid));
        cv::Mat q1(planes[i], cv::Rect(xMid, 0,    xMid, yMid));
        cv::Mat q2(planes[i], cv::Rect(0,    yMid, xMid, yMid));
        cv::Mat q3(planes[i], cv::Rect(xMid, yMid, xMid, yMid));

        q0.copyTo(tmp);
        q3.copyTo(q0);
        tmp.copyTo(q3);

        q1.copyTo(tmp);
        q2.copyTo(q1);
        tmp.copyTo(q2);
    }

    cv::merge( planes, image );

}
    

} }
