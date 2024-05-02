#pragma once
#include <cute/layout.hpp>
#include <future>
#include <memory>
#include <tuple>
#include <vector>

#include "device.h"
#include "dtype.h"
#include "task.h"

namespace dllm {

template <typename T, int N, template <typename...> class Template,
          typename... Args>
struct repeat_type {
  static_assert(N >= 0);
  using type = typename repeat_type<T, N - 1, Template, T, Args...>::type;
};

template <typename T, template <typename...> class Template, typename... Args>
struct repeat_type<T, 0, Template, Args...> {
  using type = Template<Args...>;
};

template <typename T, int N, template <typename...> class Template,
          typename... Args>
struct repeat_last_1_type {
  static_assert(N >= 0);
  using type =
      typename repeat_last_1_type<T, N - 1, Template, T, Args...>::type;
};

template <typename T, template <typename...> class Template, typename... Args>
struct repeat_last_1_type<T, 0, Template, Args...> {
  using type = Template<Args..., cute::_1>;
};

template <int N>
struct Tensor {
  using Shape = typename repeat_type<int, N, cute::Shape>::type;
  using Stride = typename repeat_last_1_type<int, N - 1, cute::Stride>::type;
  using Layout = cute::Layout<Shape, Stride>;

  Tensor(const void *data, Layout layout, Dtype dtype, DeviceType deviceType,
         std::shared_ptr<Future> future = {})
      : data_{data},
        layout{layout},
        dtype{dtype},
        deviceType{deviceType},
        future{std::move(future)} {}

  void waitFutureIfValid() const {
    if (future != nullptr && future->valid()) {
      future->wait();
    }
  }

  void *data() { return const_cast<void *>(data_); }

  const void *data() const { return data_; }

 public:
  Layout layout;
  Dtype dtype;
  DeviceType deviceType;
  std::shared_ptr<Future> future;

 private:
  const void *data_;
};

using Tensor1D = Tensor<1>;
using Tensor2D = Tensor<2>;  // (Row, Col)
using Tensor3D = Tensor<3>;  // (Batch, Sequence, Feature)
using Tensor4D = Tensor<4>;
}  // namespace dllm
