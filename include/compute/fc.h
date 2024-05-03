#pragma once
#include <cublas_v2.h>

#include "tensor.h"

namespace dllm::compute::FcNoBias {
Task forward(const std::shared_ptr<Tensor2D> &y,
             const std::shared_ptr<const Tensor2D> &x,
             const std::shared_ptr<const Tensor2D> &w,
             cublasComputeType_t computeType);

Task backwardW(const std::shared_ptr<Tensor2D> &dw,
               const std::shared_ptr<const Tensor2D> &dy,
               const std::shared_ptr<const Tensor2D> &x,
               cublasComputeType_t computeType);

Task backwardX(const std::shared_ptr<Tensor2D> &dx,
               const std::shared_ptr<const Tensor2D> &dy,
               const std::shared_ptr<const Tensor2D> &w,
               cublasComputeType_t computeType);
}  // namespace dllm::compute::FcNoBias
