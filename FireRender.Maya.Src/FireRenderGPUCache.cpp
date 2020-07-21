
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

void FireRenderGPUCache::clear()
{
	elements.clear();
	FireRenderGPUCache::clear();
}

// this function is identical to one in FireRenderMesh! 
// TODO: move it to common parent class!
void FireRenderGPUCache::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (auto scene = context()->GetScene())
	{
		for (auto element : elements)
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
		for (auto element : elements)
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
	}
}

