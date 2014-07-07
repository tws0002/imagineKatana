#include "sg_location_processor.h"

#include <stdio.h>

#include <RenderOutputUtils/RenderOutputUtils.h>

#ifndef STAND_ALONE
#include "objects/mesh.h"
#include "objects/primitives/sphere.h"

#include "geometry/standard_geometry_operations.h"
#include "geometry/standard_geometry_instance.h"

#include "geometry/compact_geometry_instance.h"
#include "geometry/compact_geometry_operations.h"

#include "lights/light.h"
#endif

#ifndef STAND_ALONE
SGLocationProcessor::SGLocationProcessor(Scene& scene) : m_applyMaterials(true), m_scene(scene), m_useTextures(true), m_enableSubd(true),
	m_useCompactGeometry(true)
{
}
#else
SGLocationProcessor::SGLocationProcessor() : m_applyMaterials(true), m_useTextures(true), m_enableSubd(true)
{
}
#endif

void SGLocationProcessor::processSG(FnKat::FnScenegraphIterator rootIterator)
{

}

void SGLocationProcessor::processSGForceExpand(FnKat::FnScenegraphIterator rootIterator)
{
	processLocationRecursive(rootIterator);
}

void SGLocationProcessor::processLocationRecursive(FnKat::FnScenegraphIterator iterator)
{
	std::string type = iterator.getType();

	if (type == "polymesh" || type == "subdmesh")
	{
		// treat subdmesh as polymesh for the moment...

		// TODO: use SG location delegates...

		if (m_useCompactGeometry)
		{
			processGeometryPolymeshCompact(iterator);
		}
		else
		{
			processGeometryPolymeshStandard(iterator);
		}

		return;
	}

	if (type == "instance")
	{

	}
	else if (type == "sphere" || type == "nurbspatch") // hack, but works for now...
	{
		processSphere(iterator);
		return;
	}
	else if (type == "light")
	{
		processLight(iterator);
		return;
	}

	FnKat::FnScenegraphIterator child = iterator.getFirstChild();
	for (; child.isValid(); child = child.getNextSibling())
	{
		processLocationRecursive(child);
	}
}

void SGLocationProcessor::processGeometryPolymeshStandard(FnKat::FnScenegraphIterator iterator)
{
	// get the geometry attributes group
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		std::string name = iterator.getFullName();
		fprintf(stderr, "Warning: polymesh '%s' does not have a 'geometry' attribute...\n", name.c_str());
		return;
	}

	StandardGeometryInstance* pNewGeoInstance = new StandardGeometryInstance();

	std::deque<Point>& aPoints = pNewGeoInstance->getPoints();

	// copy across the points...

	FnKat::GroupAttribute pointAttribute = geometryAttribute.getChildByName("point");

	// linear list of components of Vec3 points
	FnKat::FloatAttribute pAttr = pointAttribute.getChildByName("P");

	FnKat::FloatConstVector sampleData = pAttr.getNearestSample(0.0f);

	unsigned int numItems = sampleData.size();

	// convert to Point items
	for (unsigned int i = 0; i < numItems; i += 3)
	{
		float x = sampleData[i];
		float y = sampleData[i + 1];
		float z = sampleData[i + 2];

		aPoints.push_back(Point(x, y, z));
	}

	FnKat::IntAttribute uvIndexAttribute;
	bool hasUVs = false;
	bool indexedUVs = false;
	// copy any UVs
	FnKat::GroupAttribute stAttribute = iterator.getAttribute("geometry.arbitrary.st", true);
	if (stAttribute.isValid())
	{
		FnKat::RenderOutputUtils::ArbitraryOutputAttr arbitraryAttribute("st", stAttribute, "polymesh", geometryAttribute);

		if (arbitraryAttribute.isValid())
		{
			FnKat::FloatAttribute uvItemAttribute;
			if (arbitraryAttribute.hasIndexedValueAttr())
			{
				uvIndexAttribute = arbitraryAttribute.getIndexAttr(true);
				uvItemAttribute = arbitraryAttribute.getIndexedValueAttr();
				indexedUVs = true;
			}
			else
			{
				// otherwise, just build the list of vertices sequentially later
				uvItemAttribute = arbitraryAttribute.getValueAttr();
			}

			if (uvItemAttribute.isValid())
			{
				hasUVs = true;
				std::deque<UV>& aUVs = pNewGeoInstance->getUVs();

				FnKat::FloatConstVector uvlist = uvItemAttribute.getNearestSample(0);
				unsigned int numItems = uvlist.size();

				for (unsigned int i = 0; i < numItems; i += 2)
				{
					const float& u = uvlist[i];
					const float& v = uvlist[i + 1];

					aUVs.push_back(UV(u, v));
				}
			}
		}
	}


	// work out the faces...
	FnKat::GroupAttribute polyAttribute = geometryAttribute.getChildByName("poly");
	FnKat::IntAttribute polyStartIndexAttribute = polyAttribute.getChildByName("startIndex");
	FnKat::IntAttribute vertexListAttribute = polyAttribute.getChildByName("vertexList");

	unsigned int numFaces = polyStartIndexAttribute.getNumberOfTuples() - 1;
	FnKat::IntConstVector polyStartIndexAttributeValue = polyStartIndexAttribute.getNearestSample(0.0f);
	FnKat::IntConstVector vertexListAttributeValue = vertexListAttribute.getNearestSample(0.0f);

	std::deque<Face>& aFaces = pNewGeoInstance->getFaces();
	unsigned int currentVertexIndex = 0;

	if (hasUVs && indexedUVs)
	{
		// we could just generate the UV indices in the un-indexed case and use the same code-path for both
		// like the Arnold plugin does, but that's extra memory allocation which profiling seems to show
		// is slower, probably because we're doing allocation for the UVs on the Imagine side in Face::addUV()
		// anyway...
		// Once this is converted to use CompactGeometryInstance, this won't be an issue any more...

		FnKat::IntConstVector uvIndices = uvIndexAttribute.getNearestSample(0.0f);

		// the last item is extra, and doesn't designate an actual poly, so we can simply do...
		for (unsigned int i = 0; i < numFaces; i++)
		{
			unsigned int numVertices;
			if (i + 1 < numFaces)
			{
				numVertices = polyStartIndexAttributeValue[i + 1] - polyStartIndexAttributeValue[i];
			}
			else
			{
				// last one...
				numVertices = vertexListAttribute.getNumberOfTuples() - polyStartIndexAttributeValue[i];
			}

			Face newFace(numVertices);

			// now use this number to work out how many vertices we need for this face
			for (unsigned j = 0; j < numVertices; j++)
			{
				unsigned int vertexIndex = vertexListAttributeValue[currentVertexIndex];

				newFace.addVertex(vertexIndex);

				unsigned int uvIndex = uvIndices[currentVertexIndex++];

				newFace.addUV(uvIndex);
			}

			// for the moment do this, but we should read the normals directly from the geometry attribute in Katana really
			// as we can't get per-mesh crease threshold info from Katana and there are occasionally back-facing issues...
			newFace.calculateNormal(pNewGeoInstance);

			aFaces.push_back(newFace);
		}
	}
	else
	{
		// the last item is extra, and doesn't designate an actual poly, so we can simply do...
		for (unsigned int i = 0; i < numFaces; i++)
		{
			unsigned int numVertices;
			if (i + 1 < numFaces)
			{
				numVertices = polyStartIndexAttributeValue[i + 1] - polyStartIndexAttributeValue[i];
			}
			else
			{
				// last one...
				numVertices = vertexListAttribute.getNumberOfTuples() - polyStartIndexAttributeValue[i];
			}

			Face newFace(numVertices);

			if (hasUVs)
			{
				// we're unindexed, so we should have one UV per vertex...

				// now use this number to work out how many vertices we need for this face
				for (unsigned j = 0; j < numVertices; j++)
				{
					unsigned int vertexIndex = vertexListAttributeValue[currentVertexIndex++];

					newFace.addVertex(vertexIndex);
					newFace.addUV(vertexIndex);
				}
			}
			else
			{
				// now use this number to work out how many vertices we need for this face
				for (unsigned j = 0; j < numVertices; j++)
				{
					unsigned int vertexIndex = vertexListAttributeValue[currentVertexIndex++];

					newFace.addVertex(vertexIndex);
					newFace.addUV(vertexIndex);
				}
			}

			// for the moment do this, but we should read the normals directly from the geometry attribute in Katana really
			// as we can't get per-mesh crease threshold info from Katana and there are occasionally back-facing issues...
			newFace.calculateNormal(pNewGeoInstance);

			aFaces.push_back(newFace);
		}
	}

	unsigned int geoBuildFlags = GeometryInstance::GEO_BUILD_TESSELATE;

	geoBuildFlags |= GeometryInstance::GEO_BUILD_CALC_VERT_NORMALS;

	if (hasUVs)
	{
		pNewGeoInstance->setHasPerVertexUVs(true);
	}

	FnKat::DoubleAttribute boundAttr = iterator.getAttribute("bound");
	if (boundAttr.isValid())
	{
		FnKat::DoubleConstVector doubleValues = boundAttr.getNearestSample(0.0f);
		BoundaryBox bbox;
		bbox.getMinimum() = Vector(doubleValues.at(0), doubleValues.at(2), doubleValues.at(4));
		bbox.getMaximum() = Vector(doubleValues.at(1), doubleValues.at(3), doubleValues.at(5));
		pNewGeoInstance->setBoundaryBox(bbox);
	}
	else
	{
		// strictly-speaking, this means that the attributes are wrong, so we should probably ignore it, but
		// on the basis that we're force-expanding everything currently anyway...
		geoBuildFlags |= GeometryInstance::GEO_BUILD_CALC_BBOX;
	}

	pNewGeoInstance->setGeoBuildFlags(geoBuildFlags);

	Mesh* pNewMeshObject = new Mesh();

	pNewMeshObject->setGeometryInstance(pNewGeoInstance);

	FnKat::GroupAttribute materialAttrib = m_materialHelper.getMaterialForLocation(iterator);
	std::string materialHash = m_materialHelper.getMaterialHash(materialAttrib);

	// see if we've got it already
	Material* pMaterial = m_materialHelper.getExistingMaterial(materialHash);

	if (!pMaterial)
	{
		// create it
		pMaterial = m_materialHelper.createNewMaterial(materialHash, materialAttrib);
	}

	pNewMeshObject->setMaterial(pMaterial);

	// do transform

	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(iterator);

	std::set<float> sampleTimes;
	sampleTimes.insert(0);
	std::vector<float> relevantSampleTimes;
	std::copy(sampleTimes.begin(), sampleTimes.end(), std::back_inserter(relevantSampleTimes));

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	const double* pMatrix = xforms[0].getValues();

	pNewMeshObject->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_scene.addObject(pNewMeshObject, false, false);
}

#define FAST 0

void SGLocationProcessor::processGeometryPolymeshCompact(FnKat::FnScenegraphIterator iterator)
{
	// get the geometry attributes group
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		std::string name = iterator.getFullName();
		fprintf(stderr, "Warning: polymesh '%s' does not have a 'geometry' attribute...\n", name.c_str());
		return;
	}

	CompactGeometryInstance* pNewGeoInstance = new CompactGeometryInstance();

	std::vector<Point>& aPoints = pNewGeoInstance->getPoints();

	// copy across the points...

	FnKat::GroupAttribute pointAttribute = geometryAttribute.getChildByName("point");

	// linear list of components of Vec3 points
	FnKat::FloatAttribute pAttr = pointAttribute.getChildByName("P");

	FnKat::FloatConstVector sampleData = pAttr.getNearestSample(0.0f);

	unsigned int numItems = sampleData.size();

#if FAST
	aPoints.resize(numItems / 3);
	// convert to Point items
	unsigned int pointCount = 0;
	for (unsigned int i = 0; i < numItems; i += 3)
	{
		Point& point = aPoints[pointCount++];
		point.x = sampleData[i];
		point.y = sampleData[i + 1];
		point.z = sampleData[i + 2];
	}
#else
	aPoints.reserve(numItems / 3);
	// convert to Point items
	for (unsigned int i = 0; i < numItems; i += 3)
	{
		float x = sampleData[i];
		float y = sampleData[i + 1];
		float z = sampleData[i + 2];

		aPoints.push_back(Point(x, y, z));
	}
#endif

	// work out the faces...
	FnKat::GroupAttribute polyAttribute = geometryAttribute.getChildByName("poly");
	FnKat::IntAttribute polyStartIndexAttribute = polyAttribute.getChildByName("startIndex");
	FnKat::IntAttribute vertexListAttribute = polyAttribute.getChildByName("vertexList");

	unsigned int numFaces = polyStartIndexAttribute.getNumberOfTuples() - 1;
	FnKat::IntConstVector polyStartIndexAttributeValue = polyStartIndexAttribute.getNearestSample(0.0f);
	FnKat::IntConstVector vertexListAttributeValue = vertexListAttribute.getNearestSample(0.0f);

	std::vector<uint32_t>& aPolyOffsets = pNewGeoInstance->getPolygonOffsets();

	unsigned int lastOffset = 0;

	aPolyOffsets.reserve(numFaces);

	// Imagine's CompactGeometryInstance assumes 0 is the first starting index, whereas Katana
	// specifies this and not the last one, so we need to ignore the first one, and add an extra on the end
	for (unsigned int i = 0; i < numFaces; i++)
	{
		unsigned int numVertices;
		if (i + 1 < numFaces)
		{
			numVertices = polyStartIndexAttributeValue[i + 1] - polyStartIndexAttributeValue[i];
		}
		else
		{
			// last one...
			numVertices = vertexListAttribute.getNumberOfTuples() - polyStartIndexAttributeValue[i];
		}

		unsigned int polyOffset = lastOffset + numVertices;
		aPolyOffsets.push_back(polyOffset);

		lastOffset += numVertices;
	}

	// we *could* just memcpy these across directly despite the differing sign type between Katana and Imagine,
	// as long as we're only using 31 bits of the value, but it's a bit hacky, so...
	std::vector<uint32_t>& aPolyIndices = pNewGeoInstance->getPolygonIndices();
	unsigned int numIndices = vertexListAttributeValue.size();
	aPolyIndices.resize(numIndices);
	for (unsigned int i = 0; i < numIndices; i++)
	{
		const int& value = vertexListAttributeValue[i];
		aPolyIndices[i] = (uint32_t)value;
	}

	FnKat::IntAttribute uvIndexAttribute;
	bool hasUVs = false;
	bool indexedUVs = false;
	// copy any UVs
	FnKat::GroupAttribute stAttribute = iterator.getAttribute("geometry.arbitrary.st", true);
	if (stAttribute.isValid())
	{
		FnKat::RenderOutputUtils::ArbitraryOutputAttr arbitraryAttribute("st", stAttribute, "polymesh", geometryAttribute);

		if (arbitraryAttribute.isValid())
		{
			FnKat::FloatAttribute uvItemAttribute;
			if (arbitraryAttribute.hasIndexedValueAttr())
			{
				uvIndexAttribute = arbitraryAttribute.getIndexAttr(true);
				uvItemAttribute = arbitraryAttribute.getIndexedValueAttr();

				indexedUVs = true;
			}
			else
			{
				// we'll generate them later...
			}

			if (uvItemAttribute.isValid())
			{
				hasUVs = true;
				std::vector<UV>& aUVs = pNewGeoInstance->getUVs();

				FnKat::FloatConstVector uvlist = uvItemAttribute.getNearestSample(0);
				unsigned int numItems = uvlist.size();

				aUVs.resize(numItems / 2);

				std::vector<UV>::iterator itUV = aUVs.begin();
				for (unsigned int i = 0; i < numItems; i += 2)
				{
					UV& uv = *itUV++;
					uv.u = uvlist[i];
					uv.v = uvlist[i + 1];
				}
			}
		}
	}

	// set any UV indicies if necessary
	if (hasUVs)
	{
		if (indexedUVs)
		{
			// if indexed, get hold the uv indicies list
			FnKat::IntConstVector uvIndicesValue = uvIndexAttribute.getNearestSample(0.0f);

#if FAST
			// this isn't technically correct, but as long as we're only using 31 bits, will work...
			unsigned int numUVIndices = uvIndicesValue.size();
			uint32_t* pUVIndices = new uint32_t[numUVIndices];
			memcpy(pUVIndices, uvIndicesValue.data(), numUVIndices * sizeof(uint32_t));

			pNewGeoInstance->setUVIndicesRaw(pUVIndices, numUVIndices);
#else
			// we *could* just memcpy these across directly despite the differing sign type between Katana and Imagine,
			// as long as we're only using 31 bits of the value, but it's a bit hacky, so...
			unsigned int numUVIndices = uvIndicesValue.size();
			std::vector<uint32_t> aUVIndices;
			aUVIndices.resize(numUVIndices);
			for (unsigned int i = 0; i < numUVIndices; i++)
			{
				const int& value = uvIndicesValue[i];

				aUVIndices[i] = (uint32_t)value;
			}

			pNewGeoInstance->setUVIndices(aUVIndices);
#endif
		}
		else
		{
			// just make a sequential list of UV indices
#if FAST
			uint32_t* pUVIndices = new uint32_t[numIndices];

			for (unsigned int i = 0; i < numIndices; i++)
			{
				pUVIndices[i] = i;
			}

			pNewGeoInstance->setUVIndicesRaw(pUVIndices, numIndices);
#else
			std::vector<uint32_t> aUVIndices;
			aUVIndices.resize(numIndices);

			for (unsigned int i = 0; i < numIndices; i++)
			{
				aUVIndices[i] = i;
			}

			pNewGeoInstance->setUVIndices(aUVIndices);
#endif
		}

		// otherwise if we're not indexed, just set that we're using VertexUVs, and the overall Poly indicies will be used.

		pNewGeoInstance->setHasPerVertexUVs(true);
	}

	unsigned int geoBuildFlags = GeometryInstance::GEO_BUILD_TESSELATE;

	// TODO: pull in normals from poly geo if they exist...

	if (m_deduplicateVertexNormals)
	{
		geoBuildFlags |= GeometryInstance::GEO_BUILD_CALC_VERT_NORMALS_DD;
	}
	else
	{
		geoBuildFlags |= GeometryInstance::GEO_BUILD_CALC_VERT_NORMALS;
	}

	if (hasUVs)
	{
		pNewGeoInstance->setHasPerVertexUVs(true);
	}

	FnKat::DoubleAttribute boundAttr = iterator.getAttribute("bound");
	if (boundAttr.isValid())
	{
		FnKat::DoubleConstVector doubleValues = boundAttr.getNearestSample(0.0f);
		BoundaryBox bbox;
		bbox.getMinimum() = Vector(doubleValues.at(0), doubleValues.at(2), doubleValues.at(4));
		bbox.getMaximum() = Vector(doubleValues.at(1), doubleValues.at(3), doubleValues.at(5));
		pNewGeoInstance->setBoundaryBox(bbox);
	}
	else
	{
		// strictly-speaking, this means that the attributes are wrong, so we should probably ignore it, but
		// on the basis that we're force-expanding everything currently anyway...

		geoBuildFlags |= GeometryInstance::GEO_BUILD_CALC_BBOX;
	}

	geoBuildFlags |= GeometryInstance::GEO_BUILD_FREE_SOURCE_DATA;

	pNewGeoInstance->setGeoBuildFlags(geoBuildFlags);

	Mesh* pNewMeshObject = new Mesh();

	pNewMeshObject->setGeometryInstance(pNewGeoInstance);

	FnKat::GroupAttribute materialAttrib = m_materialHelper.getMaterialForLocation(iterator);
	std::string materialHash = m_materialHelper.getMaterialHash(materialAttrib);

	// see if we've got it already
	Material* pMaterial = m_materialHelper.getExistingMaterial(materialHash);

	if (!pMaterial)
	{
		// create it
		pMaterial = m_materialHelper.createNewMaterial(materialHash, materialAttrib);
	}

	pNewMeshObject->setMaterial(pMaterial);

	// do transform

	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(iterator);

	std::set<float> sampleTimes;
	sampleTimes.insert(0);
	std::vector<float> relevantSampleTimes;
	std::copy(sampleTimes.begin(), sampleTimes.end(), std::back_inserter(relevantSampleTimes));

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	const double* pMatrix = xforms[0].getValues();

	pNewMeshObject->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_scene.addObject(pNewMeshObject, false, false);
}

void SGLocationProcessor::processSphere(FnKat::FnScenegraphIterator iterator)
{
	// get the geometry attributes group
	FnKat::GroupAttribute geometryAttribute = iterator.getAttribute("geometry");
	if (!geometryAttribute.isValid())
	{
		std::string name = iterator.getFullName();
		fprintf(stderr, "Warning: sphere: %s does not have a geometry attribute...\n", name.c_str());
		return;
	}

	FnKat::DoubleAttribute radiusAttr = geometryAttribute.getChildByName("radius");

	if (!radiusAttr.isValid())
		return;

	double radius = radiusAttr.getValue(1.0f, false);

	Sphere* pSphere = new Sphere((float)radius, 24);

	FnKat::GroupAttribute materialAttrib = m_materialHelper.getMaterialForLocation(iterator);
	std::string materialHash = m_materialHelper.getMaterialHash(materialAttrib);

	// see if we've got it already
	Material* pMaterial = m_materialHelper.getExistingMaterial(materialHash);

	if (!pMaterial)
	{
		// create it
		pMaterial = m_materialHelper.createNewMaterial(materialHash, materialAttrib);
	}

	pSphere->setMaterial(pMaterial);

	// do transform

	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(iterator);

	std::set<float> sampleTimes;
	sampleTimes.insert(0);
	std::vector<float> relevantSampleTimes;
	std::copy(sampleTimes.begin(), sampleTimes.end(), std::back_inserter(relevantSampleTimes));

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	const double* pMatrix = xforms[0].getValues();

	pSphere->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_scene.addObject(pSphere, false, false);
}

void SGLocationProcessor::processLight(FnKat::FnScenegraphIterator iterator)
{
	FnKat::GroupAttribute lightMaterialAttrib = m_materialHelper.getMaterialForLocation(iterator);

	Light* pNewLight = m_lightHelper.createLight(lightMaterialAttrib);

	if (!pNewLight)
		return;

	FnKat::GroupAttribute xformAttr;
	xformAttr = FnKat::RenderOutputUtils::getCollapsedXFormAttr(iterator);

	std::set<float> sampleTimes;
	sampleTimes.insert(0);
	std::vector<float> relevantSampleTimes;
	std::copy(sampleTimes.begin(), sampleTimes.end(), std::back_inserter(relevantSampleTimes));

	FnKat::RenderOutputUtils::XFormMatrixVector xforms;

	bool isAbsolute = false;
	FnKat::RenderOutputUtils::calcXFormsFromAttr(xforms, isAbsolute, xformAttr, relevantSampleTimes,
												 FnKat::RenderOutputUtils::kAttributeInterpolation_Linear);

	const double* pMatrix = xforms[0].getValues();

	pNewLight->transform().setCachedMatrix(pMatrix, true); // invert the matrix for transpose

	m_scene.addObject(pNewLight, false, false);
}
