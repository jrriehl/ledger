#pragma once
//------------------------------------------------------------------------------
//
//   Copyright 2018-2020 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "ml/ops/ops.hpp"

#include <cassert>
#include <utility>
#include <vector>

namespace fetch {
namespace ml {
namespace ops {

template <class T>
class Reshape : public fetch::ml::ops::Ops<T>
{
public:
  using TensorType    = T;
  using SizeType      = fetch::math::SizeType;
  using VecTensorType = typename Ops<T>::VecTensorType;
  using SPType        = OpReshapeSaveableParams<T>;
  using MyType        = Reshape<TensorType>;

  explicit Reshape(std::vector<SizeType> new_shape);

  explicit Reshape(SPType const &sp);

  ~Reshape() override = default;

  std::shared_ptr<OpsSaveableParams> GetOpSaveableParams() override;

  std::shared_ptr<fetch::ml::ops::Ops<TensorType>> MakeSharedCopy(
      std::shared_ptr<fetch::ml::ops::Ops<TensorType>> me) override;

  void Forward(VecTensorType const &inputs, TensorType &output) override;

  std::vector<TensorType> Backward(VecTensorType const &inputs,
                                   TensorType const &   error_signal) override;

  std::vector<SizeType> ComputeOutputShape(VecTensorType const &inputs) const override;

  static constexpr OpType OpCode()
  {
    return OpType::OP_RESHAPE;
  }
  static constexpr char const *DESCRIPTOR = "Reshape";

  OpType OperationType() const override  // TODO(ML-466) : move implementation to .cpp
  {
    return this->OpCode();
  }
  char const *Descriptor() const override  // TODO(ML-466) : move implementation to .cpp
  {
    return DESCRIPTOR;
  }

  OperationsCount ChargeForward() const override;

private:
  std::vector<SizeType> new_shape_;
  SizeType              new_size_{0};
};

}  // namespace ops
}  // namespace ml
}  // namespace fetch
