/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                          License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

//#include "precomp.hpp"
#include "blenders.hpp"
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/stitching/detail/util.hpp>

using namespace std;

namespace cv {
namespace detail {

static const float WEIGHT_EPS = 1e-5f;

Ptr<RhoanaBlender> RhoanaBlender::createDefault(int type, bool try_gpu)
{
    if (type == NO)
        return makePtr<RhoanaBlender>();
    if (type == FEATHER)
        return makePtr<RhoanaFeatherBlender>();
    if (type == MULTI_BAND)
        return makePtr<RhoanaMultiBandBlender>(try_gpu);
#if (CV_VERSION_MAJOR >= 4)
    CV_Error(cv::Error::StsBadArg, "unsupported blending method");
#else
    CV_Error(CV_StsBadArg, "unsupported blending method");
#endif
    return Ptr<RhoanaBlender>();
}


void RhoanaBlender::prepare(const vector<Point> &corners, const vector<Size> &sizes)
{
    prepare(resultRoi(corners, sizes));
}


void RhoanaBlender::prepare(Rect dst_roi)
{
    ///dst_.create(dst_roi.size(), CV_16SC3);
    dst_.create(dst_roi.size(), CV_16SC1);
    dst_.setTo(Scalar::all(0));
    dst_mask_.create(dst_roi.size(), CV_8U);
    dst_mask_.setTo(Scalar::all(0));
    dst_roi_ = dst_roi;
}


void RhoanaBlender::feed(InputArray _img, InputArray _mask, Point tl)
{
    Mat img = _img.getMat();
    Mat mask = _mask.getMat();
    Mat dst = dst_.getMat(ACCESS_RW);
    Mat dst_mask = dst_mask_.getMat(ACCESS_RW);

    ///CV_Assert(img.type() == CV_16SC3);
    CV_Assert(img.type() == CV_16SC1);
    CV_Assert(mask.type() == CV_8U);
    int dx = tl.x - dst_roi_.x;
    int dy = tl.y - dst_roi_.y;

    for (int y = 0; y < img.rows; ++y)
    {
        ///const Point3_<short> *src_row = img.ptr<Point3_<short> >(y);
        ///Point3_<short> *dst_row = dst_.ptr<Point3_<short> >(dy + y);
        const short *src_row = img.ptr<short>(y);
        short *dst_row = dst.ptr<short>(dy + y);
        const uchar *mask_row = mask.ptr<uchar>(y);
        uchar *dst_mask_row = dst_mask.ptr<uchar>(dy + y);

        for (int x = 0; x < img.cols; ++x)
        {
            if (mask_row[x])
                dst_row[dx + x] = src_row[x];
            dst_mask_row[dx + x] |= mask_row[x];
        }
    }
}


void RhoanaBlender::blend(InputOutputArray dst, InputOutputArray dst_mask)
{
//    dst_.setTo(Scalar::all(0), dst_mask_ == 0);
//    dst = dst_;
//    dst_mask = dst_mask_;
//    dst_.release();
//    dst_mask_.release();
    UMat mask;
    compare(dst_mask_, 0, mask, CMP_EQ);
    dst_.setTo(Scalar::all(0), mask);
    dst.assign(dst_);
    dst_mask.assign(dst_mask_);
    dst_.release();
    dst_mask_.release();
}


void RhoanaFeatherBlender::prepare(Rect dst_roi)
{
    RhoanaBlender::prepare(dst_roi);
    dst_weight_map_.create(dst_roi.size(), CV_32F);
    dst_weight_map_.setTo(0);
}


void RhoanaFeatherBlender::feed(InputArray _img, InputArray mask, Point tl)
{
    Mat img = _img.getMat();
    Mat dst = dst_.getMat(ACCESS_RW);

    ///CV_Assert(img.type() == CV_16SC3);
    CV_Assert(img.type() == CV_16SC1);
    CV_Assert(mask.type() == CV_8U);

    createWeightMap(mask, sharpness_, weight_map_);
    Mat weight_map = weight_map_.getMat(ACCESS_READ);
    Mat dst_weight_map = dst_weight_map_.getMat(ACCESS_RW);

    int dx = tl.x - dst_roi_.x;
    int dy = tl.y - dst_roi_.y;

    for (int y = 0; y < img.rows; ++y)
    {
        ///const Point3_<short>* src_row = img.ptr<Point3_<short> >(y);
        ///Point3_<short>* dst_row = dst_.ptr<Point3_<short> >(dy + y);
        const short* src_row = img.ptr<short>(y);
        short* dst_row = dst.ptr<short>(dy + y);
        const float* weight_row = weight_map.ptr<float>(y);
        float* dst_weight_row = dst_weight_map.ptr<float>(dy + y);

        for (int x = 0; x < img.cols; ++x)
        {
            ///dst_row[dx + x].x += static_cast<short>(src_row[x].x * weight_row[x]);
            ///dst_row[dx + x].y += static_cast<short>(src_row[x].y * weight_row[x]);
            ///dst_row[dx + x].z += static_cast<short>(src_row[x].z * weight_row[x]);
            dst_row[dx + x] += static_cast<short>(src_row[x] * weight_row[x]);
            dst_weight_row[dx + x] += weight_row[x];
        }
    }
}


void RhoanaFeatherBlender::blend(InputOutputArray dst, InputOutputArray dst_mask)
{
    normalizeUsingWeightMap(dst_weight_map_, dst_);
    //dst_mask_ = dst_weight_map_ > WEIGHT_EPS;
    compare(dst_weight_map_, WEIGHT_EPS, dst_mask_, CMP_GT);
    RhoanaBlender::blend(dst, dst_mask);
}


Rect RhoanaFeatherBlender::createWeightMaps(const vector<UMat> &masks, const vector<Point> &corners,
                                      vector<UMat> &weight_maps)
{
    weight_maps.resize(masks.size());
    for (size_t i = 0; i < masks.size(); ++i)
        createWeightMap(masks[i], sharpness_, weight_maps[i]);

    Rect dst_roi = resultRoi(corners, masks);
    Mat weights_sum(dst_roi.size(), CV_32F);
    weights_sum.setTo(0);

    for (size_t i = 0; i < weight_maps.size(); ++i)
    {
        Rect roi(corners[i].x - dst_roi.x, corners[i].y - dst_roi.y,
                 weight_maps[i].cols, weight_maps[i].rows);
        add(weights_sum(roi), weight_maps[i], weights_sum(roi));
    }

    for (size_t i = 0; i < weight_maps.size(); ++i)
    {
        Rect roi(corners[i].x - dst_roi.x, corners[i].y - dst_roi.y,
                 weight_maps[i].cols, weight_maps[i].rows);
        Mat tmp = weights_sum(roi);
        tmp.setTo(1, tmp < numeric_limits<float>::epsilon());
        divide(weight_maps[i], tmp, weight_maps[i]);
    }

    return dst_roi;
}


RhoanaMultiBandBlender::RhoanaMultiBandBlender(int try_gpu, int num_bands, int weight_type)
{
    setNumBands(num_bands);
#if defined(HAVE_OPENCV_GPU) && !defined(DYNAMIC_CUDA_SUPPORT)
    can_use_gpu_ = try_gpu && gpu::getCudaEnabledDeviceCount();
#else
    (void)try_gpu;
    can_use_gpu_ = false;
#endif
    CV_Assert(weight_type == CV_32F || weight_type == CV_16S);
    weight_type_ = weight_type;
}


void RhoanaMultiBandBlender::prepare(Rect dst_roi)
{
    dst_roi_final_ = dst_roi;

    // Crop unnecessary bands
    double max_len = static_cast<double>(max(dst_roi.width, dst_roi.height));
    num_bands_ = min(actual_num_bands_, static_cast<int>(ceil(log(max_len) / log(2.0))));

    // Add border to the final image, to ensure sizes are divided by (1 << num_bands_)
    dst_roi.width += ((1 << num_bands_) - dst_roi.width % (1 << num_bands_)) % (1 << num_bands_);
    dst_roi.height += ((1 << num_bands_) - dst_roi.height % (1 << num_bands_)) % (1 << num_bands_);

    RhoanaBlender::prepare(dst_roi);

    dst_pyr_laplace_.resize(num_bands_ + 1);
    dst_pyr_laplace_[0] = dst_;

    dst_band_weights_.resize(num_bands_ + 1);
    dst_band_weights_[0].create(dst_roi.size(), weight_type_);
    dst_band_weights_[0].setTo(0);

    for (int i = 1; i <= num_bands_; ++i)
    {
        ///dst_pyr_laplace_[i].create((dst_pyr_laplace_[i - 1].rows + 1) / 2,
        ///                           (dst_pyr_laplace_[i - 1].cols + 1) / 2, CV_16SC3);
        dst_pyr_laplace_[i].create((dst_pyr_laplace_[i - 1].rows + 1) / 2,
                                   (dst_pyr_laplace_[i - 1].cols + 1) / 2, CV_16SC1);
        dst_band_weights_[i].create((dst_band_weights_[i - 1].rows + 1) / 2,
                                    (dst_band_weights_[i - 1].cols + 1) / 2, weight_type_);
        dst_pyr_laplace_[i].setTo(Scalar::all(0));
        dst_band_weights_[i].setTo(0);
    }
}


void RhoanaMultiBandBlender::feed(InputArray _img, InputArray mask, Point tl)
{
    UMat img = _img.getUMat();
    ///CV_Assert(img.type() == CV_16SC3 || img.type() == CV_8UC3);
    CV_Assert(img.type() == CV_16SC1 || img.type() == CV_8UC1);
    CV_Assert(mask.type() == CV_8U);

    // Keep source image in memory with small border
    ///int gap = 3 * (1 << num_bands_);
    int gap = 1 * (1 << num_bands_);
    Point tl_new(max(dst_roi_.x, tl.x - gap),
                 max(dst_roi_.y, tl.y - gap));
    Point br_new(min(dst_roi_.br().x, tl.x + img.cols + gap),
                 min(dst_roi_.br().y, tl.y + img.rows + gap));

    // Ensure coordinates of top-left, bottom-right corners are divided by (1 << num_bands_).
    // After that scale between layers is exactly 2.
    //
    // We do it to avoid interpolation problems when keeping sub-images only. There is no such problem when
    // image is bordered to have size equal to the final image size, but this is too memory hungry approach.
    tl_new.x = dst_roi_.x + (((tl_new.x - dst_roi_.x) >> num_bands_) << num_bands_);
    tl_new.y = dst_roi_.y + (((tl_new.y - dst_roi_.y) >> num_bands_) << num_bands_);
    int width = br_new.x - tl_new.x;
    int height = br_new.y - tl_new.y;
    width += ((1 << num_bands_) - width % (1 << num_bands_)) % (1 << num_bands_);
    height += ((1 << num_bands_) - height % (1 << num_bands_)) % (1 << num_bands_);
    br_new.x = tl_new.x + width;
    br_new.y = tl_new.y + height;
    int dy = max(br_new.y - dst_roi_.br().y, 0);
    int dx = max(br_new.x - dst_roi_.br().x, 0);
    tl_new.x -= dx; br_new.x -= dx;
    tl_new.y -= dy; br_new.y -= dy;

    int top = tl.y - tl_new.y;
    int left = tl.x - tl_new.x;
    int bottom = br_new.y - tl.y - img.rows;
    int right = br_new.x - tl.x - img.cols;

    // Create the source image Laplacian pyramid
    UMat img_with_border;
    copyMakeBorder(img, img_with_border, top, bottom, left, right,
                   BORDER_REFLECT);
    vector<UMat> src_pyr_laplace;
    if (can_use_gpu_ && img_with_border.depth() == CV_16S)
        createLaplacePyrGpu(img_with_border, num_bands_, src_pyr_laplace);
    else
        createLaplacePyr(img_with_border, num_bands_, src_pyr_laplace);

    // Create the weight map Gaussian pyramid
    UMat weight_map;
    vector<UMat> weight_pyr_gauss(num_bands_ + 1);

    if(weight_type_ == CV_32F)
    {
        mask.getUMat().convertTo(weight_map, CV_32F, 1./255.);
    }
    else// weight_type_ == CV_16S
    {
        mask.getUMat().convertTo(weight_map, CV_16S);
        //add(weight_map, 1, weight_map, mask != 0);
        UMat add_mask;
        compare(mask, 0, add_mask, CMP_NE);
        add(weight_map, Scalar::all(1), weight_map, add_mask);
    }

    copyMakeBorder(weight_map, weight_pyr_gauss[0], top, bottom, left, right, BORDER_CONSTANT);

    for (int i = 0; i < num_bands_; ++i)
        pyrDown(weight_pyr_gauss[i], weight_pyr_gauss[i + 1]);

    int y_tl = tl_new.y - dst_roi_.y;
    int y_br = br_new.y - dst_roi_.y;
    int x_tl = tl_new.x - dst_roi_.x;
    int x_br = br_new.x - dst_roi_.x;

    // Add weighted layer of the source image to the final Laplacian pyramid layer
/*
    if(weight_type_ == CV_32F)
    {
        for (int i = 0; i <= num_bands_; ++i)
        {
            for (int y = y_tl; y < y_br; ++y)
            {
                int y_ = y - y_tl;
                ///const Point3_<short>* src_row = src_pyr_laplace[i].ptr<Point3_<short> >(y_);
                ///Point3_<short>* dst_row = dst_pyr_laplace_[i].ptr<Point3_<short> >(y);
                const short* src_row = src_pyr_laplace[i].ptr<short>(y_);
                short* dst_row = dst_pyr_laplace_[i].ptr<short>(y);
                const float* weight_row = weight_pyr_gauss[i].ptr<float>(y_);
                float* dst_weight_row = dst_band_weights_[i].ptr<float>(y);

                for (int x = x_tl; x < x_br; ++x)
                {
                    int x_ = x - x_tl;
                    ///dst_row[x].x += static_cast<short>(src_row[x_].x * weight_row[x_]);
                    ///dst_row[x].y += static_cast<short>(src_row[x_].y * weight_row[x_]);
                    ///dst_row[x].z += static_cast<short>(src_row[x_].z * weight_row[x_]);
                    dst_row[x] += static_cast<short>(src_row[x_] * weight_row[x_]);
                    dst_weight_row[x] += weight_row[x_];
                }
            }
            x_tl /= 2; y_tl /= 2;
            x_br /= 2; y_br /= 2;
        }
    }
    else// weight_type_ == CV_16S
    {
        for (int i = 0; i <= num_bands_; ++i)
        {
            for (int y = y_tl; y < y_br; ++y)
            {
                int y_ = y - y_tl;
                ///const Point3_<short>* src_row = src_pyr_laplace[i].ptr<Point3_<short> >(y_);
                ///Point3_<short>* dst_row = dst_pyr_laplace_[i].ptr<Point3_<short> >(y);
                const short* src_row = src_pyr_laplace[i].ptr<short>(y_);
                short* dst_row = dst_pyr_laplace_[i].ptr<short>(y);
                const short* weight_row = weight_pyr_gauss[i].ptr<short>(y_);
                short* dst_weight_row = dst_band_weights_[i].ptr<short>(y);

                for (int x = x_tl; x < x_br; ++x)
                {
                    int x_ = x - x_tl;
                    ///dst_row[x].x += short((src_row[x_].x * weight_row[x_]) >> 8);
                    ///dst_row[x].y += short((src_row[x_].y * weight_row[x_]) >> 8);
                    ///dst_row[x].z += short((src_row[x_].z * weight_row[x_]) >> 8);
                    dst_row[x] += short((src_row[x_] * weight_row[x_]) >> 8);
                    dst_weight_row[x] += weight_row[x_];
                }
            }
            x_tl /= 2; y_tl /= 2;
            x_br /= 2; y_br /= 2;
        }
    }
*/
    for (int i = 0; i <= num_bands_; ++i)
    {
        Rect rc(x_tl, y_tl, x_br - x_tl, y_br - y_tl);
#ifdef HAVE_OPENCL
        if ( !cv::ocl::isOpenCLActivated() ||
             !ocl_MultiBandBlender_feed(src_pyr_laplace[i], weight_pyr_gauss[i],
                    dst_pyr_laplace_[i](rc), dst_band_weights_[i](rc)) )
#endif
        {
            Mat _src_pyr_laplace = src_pyr_laplace[i].getMat(ACCESS_READ);
            Mat _dst_pyr_laplace = dst_pyr_laplace_[i](rc).getMat(ACCESS_RW);
            Mat _weight_pyr_gauss = weight_pyr_gauss[i].getMat(ACCESS_READ);
            Mat _dst_band_weights = dst_band_weights_[i](rc).getMat(ACCESS_RW);
            if(weight_type_ == CV_32F)
            {
                for (int y = 0; y < rc.height; ++y)
                {
                    ///const Point3_<short>* src_row = _src_pyr_laplace.ptr<Point3_<short> >(y);
                    ///Point3_<short>* dst_row = _dst_pyr_laplace.ptr<Point3_<short> >(y);
                    const short* src_row = _src_pyr_laplace.ptr<short>(y);
                    short* dst_row = _dst_pyr_laplace.ptr<short>(y);
                    const float* weight_row = _weight_pyr_gauss.ptr<float>(y);
                    float* dst_weight_row = _dst_band_weights.ptr<float>(y);

                    for (int x = 0; x < rc.width; ++x)
                    {
                        ///dst_row[x].x += static_cast<short>(src_row[x].x * weight_row[x]);
                        ///dst_row[x].y += static_cast<short>(src_row[x].y * weight_row[x]);
                        ///dst_row[x].z += static_cast<short>(src_row[x].z * weight_row[x]);
                        ///dst_weight_row[x] += weight_row[x];
                        dst_row[x] += static_cast<short>(src_row[x] * weight_row[x]);
                        dst_weight_row[x] += weight_row[x];
                    }
                }
            }
            else // weight_type_ == CV_16S
            {
                for (int y = 0; y < y_br - y_tl; ++y)
                {
                    ///const Point3_<short>* src_row = _src_pyr_laplace.ptr<Point3_<short> >(y);
                    ///Point3_<short>* dst_row = _dst_pyr_laplace.ptr<Point3_<short> >(y);
                    const short* src_row = _src_pyr_laplace.ptr<short>(y);
                    short* dst_row = _dst_pyr_laplace.ptr<short>(y);
                    const short* weight_row = _weight_pyr_gauss.ptr<short>(y);
                    short* dst_weight_row = _dst_band_weights.ptr<short>(y);

                    for (int x = 0; x < x_br - x_tl; ++x)
                    {
                        ///dst_row[x].x += short((src_row[x].x * weight_row[x]) >> 8);
                        ///dst_row[x].y += short((src_row[x].y * weight_row[x]) >> 8);
                        ///dst_row[x].z += short((src_row[x].z * weight_row[x]) >> 8);
                        ///dst_weight_row[x] += weight_row[x];
                        dst_row[x] += short((src_row[x] * weight_row[x]) >> 8);
                        dst_weight_row[x] += weight_row[x];
                    }
                }
            }
        }
#ifdef HAVE_OPENCL
        else
        {
            CV_IMPL_ADD(CV_IMPL_OCL);
        }
#endif

        x_tl /= 2; y_tl /= 2;
        x_br /= 2; y_br /= 2;
    }
}


void RhoanaMultiBandBlender::blend(InputOutputArray dst, InputOutputArray dst_mask)
{
    cv::UMat dst_band_weights_0;
    Rect dst_rc(0, 0, dst_roi_final_.width, dst_roi_final_.height);

    for (int i = 0; i <= num_bands_; ++i)
        normalizeUsingWeightMap(dst_band_weights_[i], dst_pyr_laplace_[i]);

    if (can_use_gpu_)
        restoreImageFromLaplacePyrGpu(dst_pyr_laplace_);
    else
        restoreImageFromLaplacePyr(dst_pyr_laplace_);

/*
    dst_ = dst_pyr_laplace_[0];
    dst_ = dst_(Range(0, dst_roi_final_.height), Range(0, dst_roi_final_.width));
    dst_mask_ = dst_band_weights_[0] > WEIGHT_EPS;
    dst_mask_ = dst_mask_(Range(0, dst_roi_final_.height), Range(0, dst_roi_final_.width));
    dst_pyr_laplace_.clear();
    dst_band_weights_.clear();
*/
    dst_ = dst_pyr_laplace_[0](dst_rc);
    dst_band_weights_0 = dst_band_weights_[0];

    dst_pyr_laplace_.clear();
    dst_band_weights_.clear();

    compare(dst_band_weights_0(dst_rc), WEIGHT_EPS, dst_mask_, CMP_GT);

    RhoanaBlender::blend(dst, dst_mask);
}


//////////////////////////////////////////////////////////////////////////////
// Auxiliary functions

void normalizeUsingWeightMap(InputArray _weight, InputOutputArray _src)
{
    Mat src;
    Mat weight;
#ifdef HAVE_TEGRA_OPTIMIZATION
    src = _src.getMat();
    weight = _weight.getMat();
    if(tegra::useTegra() && tegra::normalizeUsingWeightMap(weight, src))
        return;
#endif
    src = _src.getMat();
    weight = _weight.getMat();
    ///CV_Assert(src.type() == CV_16SC3);
    CV_Assert(src.type() == CV_16SC1);

    if(weight.type() == CV_32FC1)
    {
        for (int y = 0; y < src.rows; ++y)
        {
            ///Point3_<short> *row = src.ptr<Point3_<short> >(y);
            short *row = src.ptr<short>(y);
            const float *weight_row = weight.ptr<float>(y);

            for (int x = 0; x < src.cols; ++x)
            {
                ///row[x].x = static_cast<short>(row[x].x / (weight_row[x] + WEIGHT_EPS));
                ///row[x].y = static_cast<short>(row[x].y / (weight_row[x] + WEIGHT_EPS));
                ///row[x].z = static_cast<short>(row[x].z / (weight_row[x] + WEIGHT_EPS));
                row[x] = static_cast<short>(row[x] / (weight_row[x] + WEIGHT_EPS));
            }
        }
    }
    else
    {
        CV_Assert(weight.type() == CV_16SC1);

        for (int y = 0; y < src.rows; ++y)
        {
            const short *weight_row = weight.ptr<short>(y);
            ///Point3_<short> *row = src.ptr<Point3_<short> >(y);
            short *row = src.ptr<short>(y);

            for (int x = 0; x < src.cols; ++x)
            {
                int w = weight_row[x] + 1;
                ///row[x].x = static_cast<short>((row[x].x << 8) / w);
                ///row[x].y = static_cast<short>((row[x].y << 8) / w);
                ///row[x].z = static_cast<short>((row[x].z << 8) / w);
                row[x] = static_cast<short>((row[x] << 8) / w);
            }
        }
    }
}


void createWeightMap(InputArray mask, float sharpness, InputOutputArray weight)
{
    CV_Assert(mask.type() == CV_8U);
    //distanceTransform(mask, weight, CV_DIST_L1, 3); turam
    distanceTransform(mask, weight, DIST_L1, 3);
    //threshold(weight * sharpness, weight, 1.f, 1.f, THRESH_TRUNC);
    UMat tmp;
    multiply(weight, sharpness, tmp);
    threshold(tmp, weight, 1.f, 1.f, THRESH_TRUNC);
}


void createLaplacePyr(InputArray img, int num_levels, vector<UMat> &pyr)
{
#ifdef HAVE_TEGRA_OPTIMIZATION
    cv::Mat imgMat = img.getMat();
    if(tegra::useTegra() && tegra::createLaplacePyr(imgMat, num_levels, pyr))
        return;
#endif

    pyr.resize(num_levels + 1);

    if(img.depth() == CV_8U)
    {
        if(num_levels == 0)
        {
            img.getUMat().convertTo(pyr[0], CV_16S);
            return;
        }

        UMat downNext;
        UMat current = img.getUMat();
        pyrDown(img, downNext);

        for(int i = 1; i < num_levels; ++i)
        {
            UMat lvl_up;
            UMat lvl_down;

            pyrDown(downNext, lvl_down);
            pyrUp(downNext, lvl_up, current.size());
            subtract(current, lvl_up, pyr[i-1], noArray(), CV_16S);

            current = downNext;
            downNext = lvl_down;
        }

        {
            UMat lvl_up;
            pyrUp(downNext, lvl_up, current.size());
            subtract(current, lvl_up, pyr[num_levels-1], noArray(), CV_16S);

            downNext.convertTo(pyr[num_levels], CV_16S);
        }
    }
    else
    {
        pyr[0] = img.getUMat();
        for (int i = 0; i < num_levels; ++i)
            pyrDown(pyr[i], pyr[i + 1]);
        UMat tmp;
        for (int i = 0; i < num_levels; ++i)
        {
            pyrUp(pyr[i + 1], tmp, pyr[i].size());
            subtract(pyr[i], tmp, pyr[i]);
        }
    }
}


void createLaplacePyrGpu(InputArray img, int num_levels, vector<UMat> &pyr)
{
#if defined(HAVE_OPENCV_GPU) && !defined(DYNAMIC_CUDA_SUPPORT)
    pyr.resize(num_levels + 1);

    vector<gpu::GpuMat> gpu_pyr(num_levels + 1);
    gpu_pyr[0].upload(img);
    for (int i = 0; i < num_levels; ++i)
        gpu::pyrDown(gpu_pyr[i], gpu_pyr[i + 1]);

    gpu::GpuMat tmp;
    for (int i = 0; i < num_levels; ++i)
    {
        gpu::pyrUp(gpu_pyr[i + 1], tmp);
        gpu::subtract(gpu_pyr[i], tmp, gpu_pyr[i]);
        gpu_pyr[i].download(pyr[i]);
    }

    gpu_pyr[num_levels].download(pyr[num_levels]);
#else
    (void)img;
    (void)num_levels;
    (void)pyr;
#if (CV_VERSION_MAJOR >= 4)
    CV_Error(cv::Error::StsNotImplemented, "CUDA optimization is unavailable");
#else
    CV_Error(CV_StsNotImplemented, "CUDA optimization is unavailable");
#endif
#endif
}


void restoreImageFromLaplacePyr(vector<UMat> &pyr)
{
    if (pyr.empty())
        return;
    UMat tmp;
    for (size_t i = pyr.size() - 1; i > 0; --i)
    {
        pyrUp(pyr[i], tmp, pyr[i - 1].size());
        add(tmp, pyr[i - 1], pyr[i - 1]);
    }
}


void restoreImageFromLaplacePyrGpu(vector<UMat> &pyr)
{
#if defined(HAVE_OPENCV_GPU) && !defined(DYNAMIC_CUDA_SUPPORT)
    if (pyr.empty())
        return;

    vector<gpu::GpuMat> gpu_pyr(pyr.size());
    for (size_t i = 0; i < pyr.size(); ++i)
        gpu_pyr[i].upload(pyr[i]);

    gpu::GpuMat tmp;
    for (size_t i = pyr.size() - 1; i > 0; --i)
    {
        gpu::pyrUp(gpu_pyr[i], tmp);
        gpu::add(tmp, gpu_pyr[i - 1], gpu_pyr[i - 1]);
    }

    gpu_pyr[0].download(pyr[0]);
#else
    (void)pyr;
#if (CV_VERSION_MAJOR >= 4)
    CV_Error(cv::Error::StsNotImplemented, "CUDA optimization is unavailable");
#else
    CV_Error(CV_StsNotImplemented, "CUDA optimization is unavailable");
#endif
#endif
}

} // namespace detail
} // namespace cv
