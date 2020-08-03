
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
#include <maya/MSelectionList.h>

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

bool FireRenderGPUCache::IsSelected(const MDagPath& dagPath) const
{
	MObject transformObject = dagPath.transform();
	bool isSelected = false;

	// get a list of the currently selected items 
	MSelectionList selected;
	MGlobal::getActiveSelectionList(selected);

	// iterate through the list of items returned
	for (unsigned int i = 0; i < selected.length(); ++i)
	{
		MObject obj;

		// returns the i'th selected dependency node
		selected.getDependNode(i, obj);

		if (obj == transformObject)
		{
			isSelected = true;
			break;
		}
	}

	return isSelected;
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

	frw::Shader alembicShader = GetAlembicShadingEngines(Object());

	if (auto scene = context()->GetScene())
	{
		for (auto element : m.elements)
		{
			if (auto shape = element.shape)
			{
				scene.Attach(shape);
				shape.SetShader(alembicShader);
			}
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

void FireRenderGPUCache::ReadAlembicFile()
{
	MStatus res;

	// get name of alembic file from Maya node
	const MObject& node = Object();
	MFnDependencyNode nodeFn(node);
	MPlug plug = nodeFn.findPlug("cacheFileName", &res);
	CHECK_MSTATUS(res);

	MString cacheFilePath = plug.asString(&res);
	CHECK_MSTATUS(res);

	try
	{
		m_archive = IArchive(Alembic::AbcCoreOgawa::ReadArchive(), cacheFilePath.asChar());
	}
	catch (std::exception &e)
	{
		char error[100];
		sprintf(error, "open alembic error: %s\n", e.what());
		MGlobal::displayError(error);
		return;
	}

	if (!m_archive.valid())
		return;

	uint32_t getNumTimeSamplings = m_archive.getNumTimeSamplings();

	std::string errorMessage;
	if (m_storage.open(cacheFilePath.asChar(), errorMessage) == false)
	{
		errorMessage = "AlembicStorage::open error: " + errorMessage;
		MGlobal::displayError(errorMessage.c_str());
		return;
	}

	static int sampleIdx = 0;
	m_scene = m_storage.read(sampleIdx, errorMessage);
	if (!m_scene)
	{
		errorMessage = "sample error: " + errorMessage;
		MGlobal::displayError(errorMessage.c_str());
		return;
	}
}

frw::Shader FireRenderGPUCache::GetAlembicShadingEngines(MObject gpucacheNode)
{
	// this is implementation that returns default shader
	// - eventually we will try reading material from alembic file and fallback on this when we fail to do so
	frw::Shader placeholderShader = Scope().GetCachedShader(std::string("DefaultShaderForAlembic"));
	if (!placeholderShader)
	{
		placeholderShader = frw::Shader(context()->GetMaterialSystem(), frw::ShaderType::ShaderTypeStandard);
		placeholderShader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, {1.0f, 1.0f, 1.0f});
		placeholderShader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, {1.0f, 1.0f, 1.0f});

		Scope().SetCachedShader(std::string("DefaultShaderForAlembic"), placeholderShader);
	}

	return placeholderShader;
}

void FireRenderGPUCache::RebuildTransforms()
{
	MObject node = Object();
	MMatrix matrix = GetSelfTransform();

	MMatrix scaleM;
	scaleM.setToIdentity();
	scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
	matrix *= scaleM;
	float mfloats[4][4];
	matrix.get(mfloats);

	for (auto& element : m.elements)
	{
		if (element.shape)
		{
			element.shape.SetTransform(&mfloats[0][0]);
		}
	}
}

void FireRenderGPUCache::Rebuild()
{
	//*********************************
	// this is called every time alembic node is moved or params changed
	// optimizations will be added so that we reload file and rebuild mesh only when its necessary
	//*********************************
	RegisterCallbacks();

	// read alembic file
	bool needReadFile = m.changed.file;
	if (needReadFile)
	{
		ReadAlembicFile();
		MDagPath meshPath = DagPath();
		ReloadMesh(meshPath);
	}

	RebuildTransforms();

	attachToScene();

	m.changed.mesh = false;
	m.changed.transform = false;
	m.changed.shader = false;
	m.changed.file = false;
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

frw::Shape TranslateAlembicMesh(const RPRAlembicWrapper::PolygonMeshObject* mesh, frw::Context& context)
{
	// get indices
	const std::vector<uint32_t>& indices = mesh->indices;
	std::vector<int> vertexIndices(indices.size(), 0); // output indices of vertexes (3 for triangle and 4 for quad)

	// mesh have only triangles => simplified mesh processing
	bool isTriangleMesh = std::all_of(mesh->faceCounts.begin(), mesh->faceCounts.end(), [](int32_t f) {
		return f == 3;
	});

	// in alembic indexes are reversed compared to what RPR expects
	if (isTriangleMesh)
	{
		for (size_t idx = 0; idx < indices.size(); idx += 3)
		{
			vertexIndices[idx] = indices[idx + 2];
			vertexIndices[idx + 1] = indices[idx + 1];
			vertexIndices[idx + 2] = indices[idx];
		}
	}
	else
	{
		uint32_t idx = 0;
		size_t countIndices = indices.size();

		for (uint32_t faceCount : mesh->faceCounts)
		{
			uint32_t currIdx = idx;

			for (uint32_t idxInPolygon = 1; idxInPolygon <= faceCount; idxInPolygon++)
			{
				vertexIndices[idx] = indices[currIdx + faceCount - idxInPolygon];
				idx++;
			}
		}
	}

	// auxilary containers necessary for passing data to RPR
	const std::vector<RPRAlembicWrapper::Vector3f>& points = mesh->P;
	const std::vector<RPRAlembicWrapper::Vector3f>& normals = mesh->N;

	// It appears that no normals is valid case for alembic file; Need to discuss
	//if (normals.size() == 0)
	//{
	//	MGlobal::displayError("Alembic translator error: No normals in loaded data!");
	//	return frw::Shape();
	//}


	unsigned int uvSetCount = 0; // no uv set; at least until we will read materials from alembic

	// pass data to RPR
	frw::Shape out = context.CreateMeshEx(
		(const float*)points.data(), points.size(), sizeof(RPRAlembicWrapper::Vector3f),
		(const float*)normals.data(), normals.size(), sizeof(RPRAlembicWrapper::Vector3f),
		nullptr, 0, 0,
		uvSetCount, nullptr, nullptr, nullptr, // no textures, no UVs
		(const int*)vertexIndices.data(), sizeof(int),
		(const int*)vertexIndices.data(), sizeof(int),
		nullptr, nullptr,
		(const int*)mesh->faceCounts.data(), mesh->faceCounts.size()
	);

	return out;
}

void FireRenderGPUCache::GetShapes(std::vector<frw::Shape>& outShapes)
{
	outShapes.clear();
	frw::Context ctx = context()->GetContext();
	assert(ctx.IsValid());

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
			outShapes.back() = TranslateAlembicMesh(mesh, ctx);
		}
	}
}

void FireRenderGPUCache::OnNodeDirty()
{
	m.changed.mesh = true;
	setDirty();
}

void FireRenderGPUCache::attributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug)
{
	std::string name = plug.name().asChar();

	// check if file name was changed
	if (name.find("cacheFileName") != std::string::npos &&
		((msg | MNodeMessage::AttributeMessage::kConnectionMade) ||
		(msg | MNodeMessage::AttributeMessage::kConnectionBroken)))
	{
		m.changed.file = true;
		OnNodeDirty();
	}
}

void FireRenderGPUCache::RegisterCallbacks()
{
	FireRenderNode::RegisterCallbacks();
}

