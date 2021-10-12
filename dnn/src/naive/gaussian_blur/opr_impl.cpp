/**
 * By downloading, copying, installing or using the software you agree to this license.
 * If you do not agree to this license, do not download, install,
 * copy or use the software.
 *
 *
 *                           License Agreement
 *                For Open Source Computer Vision Library
 *                        (3-clause BSD License)
 *
 * Copyright (C) 2000-2020, Intel Corporation, all rights reserved.
 * Copyright (C) 2009-2011, Willow Garage Inc., all rights reserved.
 * Copyright (C) 2009-2016, NVIDIA Corporation, all rights reserved.
 * Copyright (C) 2010-2013, Advanced Micro Devices, Inc., all rights reserved.
 * Copyright (C) 2015-2016, OpenCV Foundation, all rights reserved.
 * Copyright (C) 2015-2016, Itseez Inc., all rights reserved.
 * Copyright (C) 2019-2020, Xperience AI, all rights reserved.
 * Third party copyrights are property of their respective owners.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 *   * Neither the names of the copyright holders nor the names of the contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * This software is provided by the copyright holders and contributors "as is" and
 * any express or implied warranties, including, but not limited to, the implied
 * warranties of merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall copyright holders or contributors be liable for any direct,
 * indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused
 * and on any theory of liability, whether in contract, strict liability,
 * or tort (including negligence or otherwise) arising in any way out of
 * the use of this software, even if advised of the possibility of such damage.
 *
 * ---------------------------------------------------------------------------
 * \file dnn/src/naive/gaussian_blur/opr_impl.cpp
 *
 * MegEngine is Licensed under the Apache License, Version 2.0 (the "License")
 *
 * Copyright (c) 2014-2021 Megvii Inc. All rights reserved.
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT ARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * This file has been modified by Megvii ("Megvii Modifications").
 * All Megvii Modifications are Copyright (C) 2014-2021 Megvii Inc. All rights reserved.
 *
 * ---------------------------------------------------------------------------
 */

#include "./opr_impl.h"

#include "src/common/cv/common.h"
#include "src/common/cv/helper.h"
#include "src/common/gaussian_blur_helper.h"
#include "src/naive/handle.h"

namespace megdnn {
namespace naive {

template <>
void GaussianBlurImpl::exec_internal<uint8_t>(
        _megdnn_tensor_in src, _megdnn_tensor_out dst) {
    auto N = src.layout.shape[0], IH = src.layout.shape[1], IW = src.layout.shape[2],
         IC = src.layout.shape[3];

    using namespace megcv;

    Size ksize = Size(param().kernel_height, param().kernel_width);
    Mat<float> kx_(1, ksize.cols(), 1);
    Mat<float> ky_(1, ksize.rows(), 1);

    gaussian_blur::createGaussianKernels<float>(
            kx_, ky_, ksize, param().sigma_x, param().sigma_y);

    uint32_t kernel_height = ky_.width();
    uint32_t kernel_width = kx_.width();
    Mat<int> kx(1, kernel_width, 1);
    Mat<int> ky(1, kernel_height, 1);
    const uint8_t bits = 8;
    for (size_t i = 0; i < kernel_height; i++) {
        ky.at(0, i, 0) = static_cast<int>(ky_.at(0, i, 0) * (1 << bits));
    }
    for (size_t i = 0; i < kernel_width; i++) {
        kx.at(0, i, 0) = static_cast<int>(kx_.at(0, i, 0) * (1 << bits));
    }

    FixedPtCastEx<int, uint8_t> cast_op(2 * bits);
    rep(n, N) rep(h, IH) rep(w, IW) rep(c, IC) {
        int val = 0;
        rep(iy, kernel_height) {
            int y = gaussian_blur::border_interpolate(
                    h + iy - kernel_height / 2, IH, param().border_mode);
            rep(ix, kernel_width) {
                int x = gaussian_blur::border_interpolate(
                        w + ix - kernel_width / 2, IW, param().border_mode);

                //! BORDER_CONSTANT or BORDER_TRANSPARENT
                if (x != -1 && y != -1) {
                    val += kx.at(0, ix, 0) * ky.at(0, iy, 0) *
                           src.ptr<uint8_t>()
                                   [n * src.layout.stride[0] +
                                    y * src.layout.stride[1] +
                                    x * src.layout.stride[2] +
                                    c * src.layout.stride[3]];
                }
            }
        }
        dst.ptr<uint8_t>()
                [n * dst.layout.stride[0] + h * dst.layout.stride[1] +
                 w * dst.layout.stride[2] + c * dst.layout.stride[3]] = cast_op(val);
    }
}

template <typename T>
void GaussianBlurImpl::exec_internal(_megdnn_tensor_in src, _megdnn_tensor_out dst) {
    auto N = src.layout.shape[0], IH = src.layout.shape[1], IW = src.layout.shape[2],
         IC = src.layout.shape[3];

    using namespace megcv;

    Size ksize = Size(param().kernel_height, param().kernel_width);
    Mat<float> kx(1, ksize.cols(), 1);
    Mat<float> ky(1, ksize.rows(), 1);

    gaussian_blur::createGaussianKernels<float>(
            kx, ky, ksize, param().sigma_x, param().sigma_y);

    uint32_t kernel_height = ky.width();
    uint32_t kernel_width = kx.width();
    uint32_t half_h = kernel_height / 2;
    uint32_t half_w = kernel_width / 2;

    rep(n, N) rep(h, IH) rep(w, IW) rep(c, IC) {
        double val = 0;
        rep(iy, kernel_height) {
            int y = gaussian_blur::border_interpolate(
                    h + iy - half_h, IH, param().border_mode);
            rep(ix, kernel_width) {
                int x = gaussian_blur::border_interpolate(
                        w + ix - half_w, IW, param().border_mode);

                //! BORDER_CONSTANT or BORDER_TRANSPARENT
                if (x != -1 && y != -1) {
                    val += kx.at(0, ix, 0) * ky.at(0, iy, 0) *
                           src.ptr<T>()
                                   [n * src.layout.stride[0] +
                                    y * src.layout.stride[1] +
                                    x * src.layout.stride[2] +
                                    c * src.layout.stride[3]];
                }
            }
        }
        dst.ptr<T>()
                [n * dst.layout.stride[0] + h * dst.layout.stride[1] +
                 w * dst.layout.stride[2] + c * dst.layout.stride[3]] =
                static_cast<T>(val);
    }
}

void GaussianBlurImpl::exec(
        _megdnn_tensor_in src, _megdnn_tensor_in dst, _megdnn_workspace /*workspace*/) {
#define cb(DType)                                                     \
    if (src.layout.dtype == DType()) {                                \
        using ctype = typename DTypeTrait<DType>::ctype;              \
        MEGDNN_DISPATCH_CPU_KERN_OPR(exec_internal<ctype>(src, dst)); \
        return;                                                       \
    }
    cb(dtype::Uint8);
    cb(dtype::Float32);
#undef cb
    megdnn_assert_internal(0);
}

}  // namespace naive
}  // namespace megdnn

// vim: syntax=cpp.doxygen
