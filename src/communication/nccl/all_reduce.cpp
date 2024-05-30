#include "communication/all_reduce.h"

#include <nccl.h>
#include <torch/csrc/autograd/generated/variable_factories.h>

#include "internal_utils.h"
#include "logger.h"
#include "nvtx_helper.h"
#include "tensor_friend.h"

namespace dllm::communication {
namespace {
constexpr ncclRedOp_t toNcclRedOp(const Operation operation) {
  switch (operation) {
    case SUM:
      return ncclSum;
    default:
      DLLM_ASSERT_TRUE(false, "unsupported operation for NCCL all reduce");
  }
}

ncclDataType_t toNcclDataType(const at::ScalarType dtype) {
  switch (dtype) {
    case at::kDouble:
      return ncclFloat64;
    case at::kFloat:
      return ncclFloat32;
    case at::kHalf:
      return ncclFloat16;
    case at::kBFloat16:
      return ncclBfloat16;
    default:
      DLLM_ASSERT_TRUE(false, "unsupported data type for NCCL all reduce");
  }
}
}  // namespace

TaskNccl AllReduce<NCCL>::run(
    const std::shared_ptr<Tensor> &tensorReceive,
    const std::shared_ptr<const ReadOnlyTensor> &tensorSend,
    const Operation operation) {
  DLLM_ASSERT_TRUE(tensorReceive->sizes() == tensorSend->sizes(),
                   "sendbuff is not the same as the recvbuff");
  auto task = TaskNccl{
      [tensorSend = tensorSend, tensorReceive = tensorReceive,
       operation = operation, futureReceive = tensorReceive->future(),
       futureSend = tensorSend->future()](const ContextNccl *context) mutable {
        DLLM_NVTX_RANGE_FN("dllm::communication::AllReduce<NCCL>::run");
        {
          const auto count = tensorSend->numel();
          util::FutureGuard guardSend{futureSend};
          util::FutureGuard guardReceive{futureReceive};
          if (!DLLM_EXTRACT_TENSOR(tensorReceive).defined()) {
            DLLM_EXTRACT_TENSOR(tensorReceive) =
                torch::empty_like(DLLM_EXTRACT_TENSOR(tensorSend));
          }
          const auto tensorSendContiguout =
              DLLM_EXTRACT_TENSOR(tensorSend).contiguous();
          if (!DLLM_EXTRACT_TENSOR(tensorReceive).is_contiguous()) {
            DLLM_EXTRACT_TENSOR(tensorReceive) =
                DLLM_EXTRACT_TENSOR(tensorReceive).contiguous();
          }
          DLLM_ASSERT_TRUE(
              DLLM_EXTRACT_TENSOR(tensorReceive).device().type() == at::kCUDA,
              "NCCL backend only support CUDA GPUs");
          DLLM_ASSERT_TRUE(
              DLLM_EXTRACT_TENSOR(tensorSend).device().type() == at::kCUDA,
              "NCCL backend only support CUDA GPUs");
          CHECK_NCCL(ncclAllReduce(
              tensorSendContiguout.data_ptr(),
              DLLM_EXTRACT_TENSOR(tensorReceive).data_ptr(), count,
              toNcclDataType(DLLM_EXTRACT_TENSOR(tensorSend).scalar_type()),
              toNcclRedOp(operation), context->ncclComm, context->cudaStream));
          CHECK_CUDART(cudaStreamSynchronize(context->cudaStream));
        }
        tensorReceive.reset();
        tensorSend.reset();
      }};
  const TaskFuture future = task.get_future();
  tensorSend->resetFuture(future);
  tensorReceive->resetFuture(future);
  tensorReceive->sizes() = tensorSend->sizes();
  return task;
}

TaskNccl AllReduce<NCCL>::runInplace(const std::shared_ptr<Tensor> &tensor,
                                     const Operation operation) {
  auto task = TaskNccl{[tensor = tensor, operation = operation,
                        future = tensor->future()](
                           const ContextNccl *context) mutable {
    DLLM_NVTX_RANGE_FN("dllm::communication::AllReduce<NCCL>::runInplace");
{
      const auto count = tensor->numel();
      util::FutureGuard guard{future};
      if (!DLLM_EXTRACT_TENSOR(tensor).is_contiguous()) {
        DLLM_EXTRACT_TENSOR(tensor) = DLLM_EXTRACT_TENSOR(tensor).contiguous();
      }
      DLLM_ASSERT_TRUE(DLLM_EXTRACT_TENSOR(tensor).device().type() == at::kCUDA,
                       "NCCL backend only support CUDA GPUs");
      CHECK_NCCL(ncclAllReduce(
          DLLM_EXTRACT_TENSOR(tensor).data_ptr(),
          DLLM_EXTRACT_TENSOR(tensor).data_ptr(), count,
          toNcclDataType(DLLM_EXTRACT_TENSOR(tensor).scalar_type()),
          toNcclRedOp(operation), context->ncclComm, context->cudaStream));
      CHECK_CUDART(cudaStreamSynchronize(context->cudaStream));
    }
    tensor.reset();
  }};
  tensor->resetFuture(task.get_future());
  return task;
}
}  // namespace dllm::communication
