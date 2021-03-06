#include <algorithm>
#include <cfloat>
#include <vector>


#include "caffe/util/math_functions.hpp"
#include "caffe/layers/unpooling_layer.hpp"

namespace caffe {

using std::min;
using std::max;

template <typename Dtype>
void UnpoolingLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  UnpoolingParameter unpool_param = this->layer_param_.unpooling_param();

  CHECK(!unpool_param.has_kernel_size() !=
    !(unpool_param.has_kernel_h() && unpool_param.has_kernel_w()))
    << "Filter size is kernel_size OR kernel_h and kernel_w; not both";
  CHECK(unpool_param.has_kernel_size() ||
    (unpool_param.has_kernel_h() && unpool_param.has_kernel_w()))
    << "For non-square filters both kernel_h and kernel_w are required.";
  CHECK((!unpool_param.has_pad() && unpool_param.has_pad_h()
      && unpool_param.has_pad_w())
      || (!unpool_param.has_pad_h() && !unpool_param.has_pad_w()))
      << "pad is pad OR pad_h and pad_w are required.";
  CHECK((!unpool_param.has_stride() && unpool_param.has_stride_h()
      && unpool_param.has_stride_w())
      || (!unpool_param.has_stride_h() && !unpool_param.has_stride_w()))
      << "Stride is stride OR stride_h and stride_w are required.";

  if (unpool_param.has_kernel_size()) {
    kernel_h_ = kernel_w_ = unpool_param.kernel_size();
  } else {
    kernel_h_ = unpool_param.kernel_h();
    kernel_w_ = unpool_param.kernel_w();
  }
  CHECK_GT(kernel_h_, 0) << "Filter dimensions cannot be zero.";
  CHECK_GT(kernel_w_, 0) << "Filter dimensions cannot be zero.";

  if (!unpool_param.has_pad_h()) {
    pad_h_ = pad_w_ = unpool_param.pad();
  } else {
    pad_h_ = unpool_param.pad_h();
    pad_w_ = unpool_param.pad_w();
  }
  //CHECK_EQ(pad_h_, 0) << "currently, only zero padding is allowed.";
  //CHECK_EQ(pad_w_, 0) << "currently, only zero padding is allowed.";
  if (!unpool_param.has_stride_h()) {
    stride_h_ = stride_w_ = unpool_param.stride();
  } else {
    stride_h_ = unpool_param.stride_h();
    stride_w_ = unpool_param.stride_w();
  }
  if (pad_h_ != 0 || pad_w_ != 0) {
    CHECK_LT(pad_h_, kernel_h_);
    CHECK_LT(pad_w_, kernel_w_);
  }
}

template <typename Dtype>
void UnpoolingLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  //CHECK_EQ(4, bottom[0]->num_axes()) << "Input must have 4 axes, "
  //    << "corresponding to (num, channels, height, width)";
  channels_ = bottom[0]->channels();
  height_ = bottom[0]->height();
  width_ = bottom[0]->width();

  unpooled_height_ = static_cast<int>((height_ - 1) * stride_h_ + kernel_h_ - 2 * pad_h_);
  unpooled_width_ = static_cast<int>((width_ - 1) * stride_w_ + kernel_w_ - 2 * pad_w_);    
  top[0]->Reshape(bottom[0]->num(), channels_, unpooled_height_,
      unpooled_width_);
}

// TODO(Yangqing): Is there a faster way to do unpooling in the channel-first
// case?
template <typename Dtype>
void UnpoolingLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  const Dtype* bottom_data = bottom[0]->cpu_data();
  Dtype* top_data = top[0]->mutable_cpu_data();
  const int top_count = top[0]->count();
  // We'll get the mask from bottom[1] if it's of size >1.
  //const bool use_bottom_mask = bottom.size() > 1;
  const Dtype* bottom_mask = NULL;

    caffe_set(top_count, Dtype(0), top_data);
    // Initialize
      bottom_mask = bottom[1]->cpu_data();
    // The main loop
    for (int n = 0; n < bottom[0]->num(); ++n) {
      for (int c = 0; c < channels_; ++c) {
        for (int ph = 0; ph < height_; ++ph) {
          for (int pw = 0; pw < width_; ++pw) {
            const int index = ph * width_ + pw;
            const int mask_index = bottom_mask[index];
            top_data[mask_index] = bottom_data[index];  
          }
        }
        // compute offset
        bottom_data += bottom[0]->offset(0, 1);
        top_data += top[0]->offset(0, 1);
        bottom_mask += bottom[1]->offset(0, 1);
    }
  }
}

template <typename Dtype>
void UnpoolingLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  if (!propagate_down[0]) {
    return;
  }
  const Dtype* top_diff = top[0]->cpu_diff();
  Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
  // Different unpooling methods. We explicitly do the switch outside the for
  // loop to save time, although this results in more codes.
  caffe_set(bottom[0]->count(), Dtype(0), bottom_diff);
  const Dtype* bottom_mask = NULL;
  bottom_mask = bottom[1]->cpu_data();
    // The main loop
    for (int n = 0; n < top[0]->num(); ++n) {
      for (int c = 0; c < channels_; ++c) {
        for (int ph = 0; ph < height_; ++ph) {
          for (int pw = 0; pw < width_; ++pw) {
            const int index = ph * width_ + pw;
            const int mask_index = bottom_mask[index];
              bottom_diff[index] = top_diff[mask_index]; 
            }
          }
        }
        // compute offset
        bottom_diff += bottom[0]->offset(0, 1);
        top_diff += top[0]->offset(0, 1);
        bottom_mask += bottom[1]->offset(0, 1);
    }
}


#ifdef CPU_ONLY
STUB_GPU(UnpoolingLayer);
#endif

INSTANTIATE_CLASS(UnpoolingLayer);

}  // namespace caffe