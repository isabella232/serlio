/*
 * Copyright 2014-2018 Esri R&D Zurich and VRBN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "../CodecMain.h"

#include "prtx/ResolveMap.h"
#include "prtx/Encoder.h"
#include "prtx/EncoderFactory.h"
#include "prtx/EncoderInfoBuilder.h"
#include "prtx/PRTUtils.h"
#include "prtx/Singleton.h"
#include "prtx/EncodePreparator.h"

#include "prt/ContentType.h"
#include "prt/InitialShape.h"

#include <string>
#include <iostream>
#include <stdexcept>


class IMayaCallbacks;

namespace detail {

struct SerializedGeometry {
	prtx::DoubleVector              coords;
	prtx::DoubleVector              normals; // uses same indexing as coords
	std::vector<prtx::DoubleVector> uvs;     // uses same indexing as coords per uv set
	std::vector<uint32_t>           counts;
	std::vector<uint32_t>           indices;

	SerializedGeometry(uint32_t uvSets) : uvs(uvSets) { }
};

// visible for tests
CODEC_EXPORTS_API SerializedGeometry serializeGeometry(const prtx::GeometryPtrVector &geometries, const std::vector<prtx::MaterialPtrVector>& materials);

} // namespace detail


class MayaEncoder : public prtx::GeometryEncoder {
public:
	MayaEncoder(const std::wstring& id, const prt::AttributeMap* options, prt::Callbacks* callbacks);
	~MayaEncoder() override = default;

public:
	void init(prtx::GenerateContext& context) override;
	void encode(prtx::GenerateContext& context, size_t initialShapeIndex) override;
	void finish(prtx::GenerateContext& context) override;

private:
	void convertGeometry(const prtx::InitialShape& initialShape,
	                     const prtx::EncodePreparator::InstanceVector& instances,
	                     IMayaCallbacks* callbacks);
};


class MayaEncoderFactory : public prtx::EncoderFactory, public prtx::Singleton<MayaEncoderFactory> {
public:
	static MayaEncoderFactory* createInstance();

	explicit MayaEncoderFactory(const prt::EncoderInfo* info) : prtx::EncoderFactory(info) { }
	~MayaEncoderFactory() override = default;

	MayaEncoder* create(const prt::AttributeMap* options, prt::Callbacks* callbacks) const override {
		return new MayaEncoder(getID(), options, callbacks);
	}
};
