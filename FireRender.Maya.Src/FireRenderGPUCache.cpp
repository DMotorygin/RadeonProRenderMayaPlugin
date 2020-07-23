
#include "FireRenderGPUCache.h"
#include "Context/FireRenderContext.h"
#include "FireRenderUtils.h"
#include "AlembicWrapper.hpp"

#include <array>
#include <algorithm>
#include <vector>
#include <iterator>
#include <istream>
#include <ostream>
#include <sstream>

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MMatrix.h>
#include <maya/MDagPath.h>

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

using namespace Alembic::Abc;
using namespace Alembic::AbcGeom;

FireRenderGPUCache::FireRenderGPUCache(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderNode(context, dagPath)
{
}

FireRenderGPUCache::~FireRenderGPUCache()
{
	FireRenderGPUCache::clear();
}

bool FireRenderGPUCache::IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const
{
	// NIY!
	return true;
}

void FireRenderGPUCache::clear()
{
	m.elements.clear();
	FireRenderObject::clear();
}

// this function is identical to one in FireRenderMesh! 
// TODO: move it to common parent class!
void FireRenderGPUCache::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (auto scene = context()->GetScene())
	{
		for (auto element : m.elements)
		{
			if (auto shape = element.shape)
				scene.Detach(shape);
		}
	}
	m_isVisible = false;
}

// this function is identical to one in FireRenderMesh! 
// TODO: move it to common parent class!
void FireRenderGPUCache::attachToScene()
{
	if (m_isVisible)
		return;

	if (auto scene = context()->GetScene())
	{
		for (auto element : m.elements)
		{
			if (auto shape = element.shape)
				scene.Attach(shape);
		}
		m_isVisible = true;
	}
}

// this function is identical to one in FireRenderMesh! 
// TODO: move it to common parent class!
void FireRenderGPUCache::Freshen()
{
	Rebuild();
	FireRenderNode::Freshen();
}

void FireRenderGPUCache::Rebuild()
{
	MStatus res;

	// get name of alembic file from Maya node
	const MObject& node = Object();
	MFnDependencyNode nodeFn(node);
	MPlug plug = nodeFn.findPlug("cacheFileName", &res);
	CHECK_MSTATUS(res);

	MString cacheFilePath = plug.asString(&res);
	CHECK_MSTATUS(res);

	//****************************************************
	// open alembic archive
	try 
	{
		m_archive = IArchive(Alembic::AbcCoreOgawa::ReadArchive(), cacheFilePath.asChar());
	}
	catch (std::exception &e) 
	{
		char error [100];
		sprintf(error, "open alembic error: %s\n", e.what());
		MGlobal::displayError(error);
		return;
	}

	if (!m_archive.valid())
		return;

	std::string errorMessage;
	if (m_storage.open(cacheFilePath.asChar(), errorMessage) == false) 
	{
		errorMessage = "AlembicStorage::open error: " + errorMessage; 
		MGlobal::displayError(errorMessage.c_str());
		return;
	}

	bool isOpened = m_storage.isOpened();
	uint32_t frameCount = m_storage.frameCount();


	static int sampleIdx = 0;
	m_scene = m_storage.read(sampleIdx, errorMessage);
	if (!m_scene)
	{
		errorMessage = "sample error: " + errorMessage;
		MGlobal::displayError(errorMessage.c_str());
		return;
	}
	//****************************************************

	MDagPath meshPath = DagPath();

	// will not be calling it always eventually
	ReloadMesh(meshPath);

	//m.changed.mesh = false;
	//m.changed.transform = false;
	//m.changed.shader = false;
}

void FireRenderGPUCache::ReloadMesh(const MDagPath& meshPath)
{
	m.elements.clear();

	// node is not visible => skip
	if (!IsMeshVisible(meshPath, this->context()))
		return;

	std::vector<frw::Shape> shapes;
	GetShapes(shapes);

	m.elements.resize(shapes.size());
	for (unsigned int i = 0; i < shapes.size(); i++)
	{
		m.elements[i].shape = shapes[i];
	}
}

frw::Shape TranslateAlembicMesh(const RPRAlembicWrapper::PolygonMeshObject* mesh)
{
	bool isTriangleMesh = std::all_of(mesh->faceCounts.begin(), mesh->faceCounts.end(), [](int32_t f) {
		return f == 3; });

	if (!isTriangleMesh)
		return frw::Shape();

	return frw::Shape();
}

void FireRenderGPUCache::GetShapes(std::vector<frw::Shape>& outShapes)
{
	outShapes.clear();

	// ensure correct input
	if (!m_scene)
		return;

	// translate alembic data into RPR shapes (to be decomposed...)
	for (auto alembicObj : m_scene->objects)
	{
		if (alembicObj->visible == false) 
			continue;

		if (RPRAlembicWrapper::PolygonMeshObject* mesh = alembicObj.as_polygonMesh())
		{
			outShapes.emplace_back();
			outShapes.back() = TranslateAlembicMesh(mesh);
		}
	}
}
