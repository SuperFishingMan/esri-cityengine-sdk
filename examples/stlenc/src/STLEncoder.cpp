/**
 * Esri CityEngine SDK Custom STL Encoder Example
 *
 * This example demonstrates the usage of the PRTX interface
 * to write custom encoders.
 *
 * See README.md in http://github.com/ArcGIS/esri-cityengine-sdk for build instructions.
 *
 * Written by Simon Haegler
 * Esri R&D Center Zurich, Switzerland
 *
 * Copyright 2014 Esri R&D Center Zurich
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ostream>
#include <sstream>

#include "boost/foreach.hpp"

#include "prtx/Shape.h"
#include "prtx/ShapeIterator.h"
#include "prtx/GenerateContext.h"
#include "prtx/Geometry.h"
#include "prtx/EncodeOptions.h"
#include "prtx/EncoderInfoBuilder.h"

#include "STLEncoder.h"


const std::wstring STLEncoder::ID			= L"com.esri.prt.examples.STLEncoder";
const std::wstring STLEncoder::NAME			= L"STL Encoder";
const std::wstring STLEncoder::DESCRIPTION	= L"Example encoder for the STL format";

const wchar_t* EO_BASE_NAME					= L"baseName";
const std::wstring STL_EXT					= L".stl";
const wchar_t* WNL							= L"\n";


/**
 * The constructor just forwards the encoder id, defaultOptions and callbacks to the geometry encoder.
 */
STLEncoder::STLEncoder(const std::wstring& id, const prt::AttributeMap* defaultOptions, prt::Callbacks* callbacks)
: prtx::GeometryEncoder(id, defaultOptions, callbacks)
{
}


STLEncoder::~STLEncoder() {
}


/**
 * Setup two namespaces for mesh and material objects and initialize the encode preprator.
 * The namespaces are used to create unique names for all mesh and material objects.
 */
void STLEncoder::init(prtx::GenerateContext& /*context*/) {
	prtx::NamePreparator::NamespacePtr nsMaterials = mNamePreparator.newNamespace();
	prtx::NamePreparator::NamespacePtr nsMeshes = mNamePreparator.newNamespace();
	mEncodePreparator = prtx::EncodePreparator::create(true, mNamePreparator, nsMeshes, nsMaterials);
}


/**
 * During encoding we collect the resulting shapes with the encode preparator.
 * In case the shape generation fails, we collect the initial shape.
 */
void STLEncoder::encode(prtx::GenerateContext& context, size_t initialShapeIndex) {
	const prtx::InitialShape* is = context.getInitialShape(initialShapeIndex);
	try {
		prtx::LeafIteratorPtr li = prtx::LeafIterator::create(context, initialShapeIndex);
		for (prtx::ShapePtr shape = li->getNext(); shape.get() != 0; shape = li->getNext()) {
			mEncodePreparator->add(context.getCache(), shape, is->getAttributeMap());
		}
	} catch(...) {
		mEncodePreparator->add(context.getCache(), *is, initialShapeIndex);
	}
}


/**
 * After all shapes have been generated, we write the actual STL file by looping over the
 * finalized geometry instances.
 */
void STLEncoder::finish(prtx::GenerateContext& /*context*/) {
	prt::SimpleOutputCallbacks* soh = dynamic_cast<prt::SimpleOutputCallbacks*>(getCallbacks());
	const std::wstring baseName = getOptions()->getString(EO_BASE_NAME);

	prtx::EncodePreparator::PreparationFlags prepFlags;
	prepFlags.instancing(false);
	prepFlags.mergeByMaterial(true);
	prepFlags.triangulate(true);
	prepFlags.mergeVertices(true);
	prepFlags.cleanupVertexNormals(true);
	prepFlags.cleanupUVs(true);
	prepFlags.processVertexNormals(prtx::VertexNormalProcessor::SET_ALL_TO_FACE_NORMALS);

	std::vector<prtx::EncodePreparator::FinalizedInstance> finalizedInstances;
	mEncodePreparator->fetchFinalizedInstances(finalizedInstances, prepFlags);

	std::wostringstream out;
	out << std::scientific;
	out << L"solid " << baseName << L"\n";

	BOOST_FOREACH(prtx::EncodePreparator::FinalizedInstance& instance, finalizedInstances) {
		BOOST_FOREACH(const prtx::MeshPtr& m, instance.getGeometry()->getMeshes()) {
			for (uint32_t fi = 0, n = m->getFaceCount(); fi < n; fi++) {
				const prtx::DoubleVector& vc = m->getVertexCoords();

				const uint32_t* fvi = m->getFaceVertexIndices(fi);
				assert(m->getFaceVertexCount(fi) == 3); // we enabled triangulation above
				uint32_t vi0 = 3 * fvi[0];
				uint32_t vi1 = 3 * fvi[1];
				uint32_t vi2 = 3 * fvi[2];

				// using first vertex normal as face normal, see call to processVertexNormals() above
				const uint32_t* fvni = m->getFaceVertexNormalIndices(fi);
				const double* fn = &m->getVertexNormalsCoords()[3 * fvni[0]];

				out << L"facet normal " << fn[0] << L" " << fn[1] << L" " << fn[2] << WNL;
				out << L"  outer loop" << WNL;
				out << L"    vertex " << vc[vi0] << L" " << vc[vi0+1] << L" " << vc[vi0+2] << WNL;
				out << L"    vertex " << vc[vi1] << L" " << vc[vi1+1] << L" " << vc[vi1+2] << WNL;
				out << L"    vertex " << vc[vi2] << L" " << vc[vi2+1] << L" " << vc[vi2+2] << WNL;
				out << L"  endloop" << WNL;
				out << L"endfacet" << WNL;
			}
		}
	}

	out << L"endsolid" << WNL;

	// let the client application write the file via callback
	std::wstring fileName = baseName + STL_EXT;
	uint64_t h = soh->open(ID.c_str(), prt::CT_GEOMETRY, fileName.c_str(), prt::SimpleOutputCallbacks::SE_UTF8);
	soh->write(h, out.str().c_str());
	soh->close(h, 0, 0);
}


/**
 * Create the STL encoder factory singleton and define the default options.
 */
STLEncoderFactory* STLEncoderFactory::createInstance() {
	prtx::EncoderInfoBuilder encoderInfoBuilder;

	encoderInfoBuilder.setID(STLEncoder::ID);
	encoderInfoBuilder.setName(STLEncoder::NAME);
	encoderInfoBuilder.setDescription(STLEncoder::DESCRIPTION);
	encoderInfoBuilder.setType(prt::CT_GEOMETRY);

	// optionally we could setup a validator
	// encoderInfoBuilder.setValidator(prtx::EncodeOptionsValidatorPtr(new TestOptionsValidator()));

	prtx::PRTUtils::AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
	amb->setString(EO_BASE_NAME, L"stl_default_name");
	encoderInfoBuilder.setDefaultOptions(amb->createAttributeMap());

	// CityEngine requires the following annotations to create an UI for an option:
	// label, order, group and description
	prtx::EncodeOptionsAnnotator eoa(encoderInfoBuilder);
	eoa.option(EO_BASE_NAME).
			setLabel(L"Base Name").
			setOrder(0.0).
			setGroup(L"General Settings", 0.0).
			setDescription(L"Sets the base name of the written STL file.");

	return new STLEncoderFactory(encoderInfoBuilder.create());
}
