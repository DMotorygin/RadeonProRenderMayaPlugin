#include "FireRenderSkyLocator.h"
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MImage.h>
#include <maya/MFileObject.h>
#include <maya/MDagModifier.h>
#include <maya/MFnTransform.h>
#include <maya/MEulerRotation.h>
#include "FireMaya.h"

using namespace std;

MTypeId FireRenderSkyLocator::id(FireMaya::TypeId::FireRenderSkyLocator);

MString FireRenderSkyLocator::drawDbClassification("drawdb/geometry/FireRenderSkyLocator");
MString FireRenderSkyLocator::drawRegistrantId("FireRenderSkyNode");

MStatus FireRenderSkyLocator::compute(const MPlug& plug, MDataBlock& data)
{
	return MS::kUnknownParameter;
}

template <class T>
void makeAttribute(T& attr )
{
	CHECK_MSTATUS(attr.setKeyable(true ));
	CHECK_MSTATUS(attr.setStorable(true ));
	CHECK_MSTATUS(attr.setReadable(true ));
	CHECK_MSTATUS(attr.setWritable(true ));
}

MStatus FireRenderSkyLocator::initialize()
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;
	MFnEnumAttribute eAttr;
	MFnMessageAttribute mAttr;

	MObject a = nAttr.create("turbidity", "tu", MFnNumericData::kFloat, 0.1f);
	makeAttribute(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(50);
	addAttribute(a);

	a = nAttr.create("intensity", "i", MFnNumericData::kFloat, 0.1f);
	makeAttribute(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(1000);
	nAttr.setSoftMax(2.0);
	addAttribute(a);

	a = mAttr.create("portal", "p");
	addAttribute(a);

	a = nAttr.createColor("filterColor", "fcol");
	makeAttribute(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.0f, 0.0f, 0.0f));
	addAttribute(a);

	a = nAttr.createColor("groundColor", "gcol");
	makeAttribute(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.4f, 0.4f, 0.4f));
	addAttribute(a);

	a = nAttr.createColor("groundAlbedo", "galb");
	makeAttribute(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.5f, 0.5f, 0.5f));
	addAttribute(a);

	a = nAttr.create("sunGlow", "g", MFnNumericData::kFloat, 2.0f);
	makeAttribute(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setMax(100.0f);
	addAttribute(a);

	a = nAttr.create("sunDiskSize", "sds", MFnNumericData::kFloat, 1.0f);
	makeAttribute(nAttr);
	nAttr.setMin(0.00);
	nAttr.setMax(10.0);
	addAttribute(a);

	a = eAttr.create("sunPositionType", "spt", SkyAttributes::kAltitudeAzimuth);
	eAttr.addField("Altitude / Azimuth", SkyAttributes::kAltitudeAzimuth);
	eAttr.addField("Time / Location", SkyAttributes::kTimeLocation);
	addAttribute(a);

	a = nAttr.create("azimuth", "az", MFnNumericData::kFloat, 0);
	makeAttribute(nAttr);
	nAttr.setMin(-360.0);
	nAttr.setMax(360.0);
	addAttribute(a);

	a = nAttr.create("altitude", "alt", MFnNumericData::kFloat, 45);
	makeAttribute(nAttr);
	nAttr.setMin(-90.0);
	nAttr.setMax(90.0);
	addAttribute(a);

	a = nAttr.create("hours", "hr", MFnNumericData::kInt, 12);
	makeAttribute(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(24);
	addAttribute(a);

	a = nAttr.create("minutes", "min", MFnNumericData::kInt, 0);
	makeAttribute(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(60);
	addAttribute(a);

	a = nAttr.create("seconds", "sc", MFnNumericData::kInt, 0);
	makeAttribute(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(60);
	addAttribute(a);

	a = nAttr.create("month", "mn", MFnNumericData::kInt, 1);
	makeAttribute(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(12);
	addAttribute(a);

	a = nAttr.create("day", "d", MFnNumericData::kInt, 1);
	makeAttribute(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(31);
	addAttribute(a);

	a = nAttr.create("year", "y", MFnNumericData::kInt, 2016);
	makeAttribute(nAttr);
	nAttr.setSoftMin(-2000);
	nAttr.setSoftMax(6000);
	addAttribute(a);

	a = nAttr.create("timeZone", "tz", MFnNumericData::kFloat, 0);
	makeAttribute(nAttr);
	nAttr.setMin(-18);
	nAttr.setMax(18);
	addAttribute(a);

	a = nAttr.create("daylightSaving", "dls", MFnNumericData::kBoolean, false);
	makeAttribute(nAttr);
	addAttribute(a);

	a = nAttr.create("latitude", "lt", MFnNumericData::kFloat, 0);
	makeAttribute(nAttr);
	nAttr.setMin(-90);
	nAttr.setMax(90);
	addAttribute(a);

	a = nAttr.create("longitude", "lg", MFnNumericData::kFloat, 0);
	makeAttribute(nAttr);
	nAttr.setMin(-180);
	nAttr.setMax(180);
	addAttribute(a);

	return MS::kSuccess;
}

void FireRenderSkyLocator::draw(M3dView& view, const MDagPath&,
	M3dView::DisplayStyle style,
	M3dView::DisplayStatus status)
{
	// Create the locator mesh if required.
	if (!m_mesh)
		m_mesh = make_unique<SkyLocatorMesh>(thisMObject());

	// Refresh and draw the mesh.
	m_mesh->refresh();
	m_mesh->glDraw(view);
}

bool FireRenderSkyLocator::isBounded() const
{
	return true;
}

MBoundingBox FireRenderSkyLocator::boundingBox() const
{
	MObject thisNode = thisMObject();
	MPoint corner1(-10.0, -10.0, -10.0);
	MPoint corner2(10.0, 10.0, 10.0);
	return MBoundingBox(corner1, corner2);
}
void* FireRenderSkyLocator::creator()
{
	return new FireRenderSkyLocator();
}

// ================================
// Viewport 2.0 override
// ================================

FireRenderSkyLocatorOverride::FireRenderSkyLocatorOverride(const MObject& obj) :
	MHWRender::MPxGeometryOverride(obj),
	m_mesh(obj),
	m_changed(true)
{
}

FireRenderSkyLocatorOverride::~FireRenderSkyLocatorOverride()
{
}

MHWRender::DrawAPI FireRenderSkyLocatorOverride::supportedDrawAPIs() const
{
#ifndef MAYA2015
	return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
#else
	return (MHWRender::kOpenGL | MHWRender::kDirectX11);
#endif
}

void FireRenderSkyLocatorOverride::updateDG()
{
	if (m_mesh.refresh())
		m_changed = true;
}

void FireRenderSkyLocatorOverride::updateRenderItems(const MDagPath& path, MHWRender::MRenderItemList& list)
{
	MHWRender::MRenderItem* mesh = NULL;

	int index = list.indexOf("locatorMesh");

	if (index < 0)
	{
		mesh = MHWRender::MRenderItem::Create("locatorMesh",
			MHWRender::MRenderItem::DecorationItem, MHWRender::MGeometry::kLines);
		mesh->setDrawMode(MHWRender::MGeometry::kAll);
		list.append(mesh);
	}
	else
		mesh = list.itemAt(index);

	if (mesh)
	{
		MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
		const MHWRender::MShaderManager* shaderManager = renderer->getShaderManager();
		MHWRender::MShaderInstance* shader = shaderManager->getStockShader(MHWRender::MShaderManager::k3dSolidShader);
		if (shader) {
			MColor c = MHWRender::MGeometryUtilities::wireframeColor(path);
			float color[] = { c.r, c.g, c.b, 1.0f };
			shader->setParameter("solidColor", color);
			mesh->setShader(shader);
			shaderManager->releaseShader(shader);
		}

		mesh->enable(true);
	}
}

void FireRenderSkyLocatorOverride::populateGeometry(
	const MHWRender::MGeometryRequirements& requirements,
	const MHWRender::MRenderItemList& renderItems,
	MHWRender::MGeometry& data)
{
	m_mesh.populateOverrideGeometry(requirements, renderItems, data);
	m_changed = false;
}

#ifndef MAYA2015

bool FireRenderSkyLocatorOverride::refineSelectionPath(const MHWRender::MSelectionInfo &  	selectInfo,
	const MHWRender::MRenderItem &  	hitItem,
	MDagPath &  	path,
	MObject &  	components,
	MSelectionMask &  	objectMask
	)
{
	return true;
}

void FireRenderSkyLocatorOverride::updateSelectionGranularity(
	const MDagPath& path,
	MHWRender::MSelectionContext& selectionContext)
{
}
#endif