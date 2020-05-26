//*****************************************************************************
// Copyright 2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************
#pragma once

#include <memory>
#include <string>

#include <inference_engine.hpp>
#include <spdlog/spdlog.h>
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow_serving/apis/prediction_service.grpc.pb.h"

#include "status.hpp"
#include "tensorinfo.hpp"

namespace ovms {

Status serializeBlobToTensorProto(
    tensorflow::TensorProto& responseOutput,
    const std::shared_ptr<TensorInfo>& networkOutput,
    InferenceEngine::Blob::Ptr blob) {
    responseOutput.Clear();
    switch (networkOutput->getPrecision()) {
        case InferenceEngine::Precision::FP32: responseOutput.set_dtype(tensorflow::DataTypeToEnum<float>::value); break;
        case InferenceEngine::Precision::I32:  responseOutput.set_dtype(tensorflow::DataTypeToEnum<int>::value); break;
        // case Precision::DOUBLE?: responseOutput.set_dtype(tensorflow::DataTypeToEnum<float>::value); // unsupported by OV?
        case InferenceEngine::Precision::I64:
        case InferenceEngine::Precision::I8:
        case InferenceEngine::Precision::U8:
        case InferenceEngine::Precision::U16:
        case InferenceEngine::Precision::Q78:
        case InferenceEngine::Precision::FP16:
        case InferenceEngine::Precision::BIN:
        case InferenceEngine::Precision::BOOL:
        case InferenceEngine::Precision::MIXED:
        case InferenceEngine::Precision::CUSTOM:
        default: {
            Status status = StatusCode::OV_UNSUPPORTED_SERIALIZATION_PRECISION;
            SPDLOG_ERROR(status.string());
            return status;
        }
    }
    responseOutput.mutable_tensor_shape()->Clear();
    for (auto dim : networkOutput->getShape()) {
        responseOutput.mutable_tensor_shape()->add_dim()->set_size(dim);
    }
    responseOutput.mutable_tensor_content()->assign((char*) blob->buffer(), blob->byteSize());
    return StatusCode::OK;
}

Status serializePredictResponse(
    InferenceEngine::InferRequest& inferRequest,
    const tensor_map_t& outputMap,
    tensorflow::serving::PredictResponse* response) {

    for (const auto& pair : outputMap) {
        auto networkOutput = pair.second;
        InferenceEngine::Blob::Ptr blob;
        try {
            blob = inferRequest.GetBlob(networkOutput->getName());
        } catch (const InferenceEngine::details::InferenceEngineException& e) {
            Status status = StatusCode::OV_INTERNAL_SERIALIZATION_ERROR;
            SPDLOG_ERROR("{}: {}", status.string(), e.what());
            return status;
        }
        auto& tensorProto = (*response->mutable_outputs())[networkOutput->getMappedName()];
        auto status = serializeBlobToTensorProto(tensorProto, networkOutput, blob);
        if (!status.ok()) {
            return status;
        }
    }

    return StatusCode::OK;
}

}  // namespace ovms
