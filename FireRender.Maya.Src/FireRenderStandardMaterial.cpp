#include "FireRenderStandardMaterial.h"
#include "FireRenderDisplacement.h"
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatVector.h>
#include <maya/MPlugArray.h>
#include <maya/MDGModifier.h>

#include "FireMaya.h"

// Comment next line to use old UberMaterial code. Should remove USE_RPRX=0 path when UberMaterial will pass all tests.
#define USE_RPRX 1

namespace
{
	namespace Attribute
	{
		MObject version;
		MObject output;
		MObject outputAlpha;

		// Diffuse
#if USE_RPRX
		MObject diffuseEnable;
#endif
		MObject diffuseColor;
#if USE_RPRX
		MObject diffuseWeight;
		MObject diffuseRoughness;
#endif

		// Reflection
		MObject reflectionEnable;
		MObject reflectionColor;
#if USE_RPRX
		MObject reflectionWeight;
		MObject reflectionRoughness;
		MObject reflectionAnisotropy;
		MObject reflectionAnisotropyRotation;
		MObject reflectionMetalMaterial;
		MObject reflectionMetalness;
#endif
		MObject reflectionIOR;
		MObject reflectionRoughnessX;	// used for upgrade v1 -> v2
#if !USE_RPRX
		MObject reflectionRotation;		// warning: not used
		MObject reflectionRoughnessY;
#endif

		// Coating
		MObject clearCoatEnable;
		MObject clearCoatColor;
		MObject clearCoatIOR;
#if USE_RPRX
		MObject clearCoatWeight;
		MObject clearCoatRoughness;
		MObject clearCoatMetalMaterial;
		MObject clearCoatMetalness;
#endif

		// Refraction
#if USE_RPRX
		MObject refractionEnable;
#endif
		MObject refractionColor;
		MObject refractionWeight;
		MObject refractionRoughness;
		MObject refractionIOR;
#if USE_RPRX
		MObject refractionLinkToReflection;
		MObject refractionThinSurface;
#endif

		// Emissive
#if USE_RPRX
		MObject emissiveEnable;
		MObject emissiveColor;
		MObject emissiveWeight;
		MObject emissiveDoubleSided;
#endif

		// Material parameters
		MObject transparencyLevel;
		MObject displacementMap;			// warning: not used in old UberShader
#if USE_RPRX
		MObject normalMap;
		MObject normalMapEnable;

		MObject transparencyEnable;
		MObject displacementEnable;
#endif
#if !USE_RPRX
		MObject transparencyColor;
#endif

#if USE_RPRX
		MObject sssEnable;
		MObject sssUseDiffuseColor;
		MObject sssColor;
		MObject sssWeight;
#endif
		MObject volumeScatter;				// scatter color
		MObject volumeTransmission;			// absorption color
		MObject volumeDensity;
		MObject volumeScatteringDirection;	//+
		MObject volumeMultipleScattering;	//+ ! single scattering
#if !USE_RPRX
		MObject volumeEnable;
		MObject volumeEmission;
#endif

		// Old attributes declared for backwards compatibility
		MObject diffuseBaseNormal;
		MObject reflectionNormal;
		MObject clearCoatNormal;
		MObject refractionNormal;
	}
}

// Attributes
void FireMaya::StandardMaterial::postConstructor()
{
	ShaderNode::postConstructor();
	setMPSafe(true);
}

void FireMaya::StandardMaterial::OnFileLoaded()
{
#if USE_RPRX
	// Execute upgrade code
	UpgradeMaterial();
#endif
}

enum
{
	VER_INITIAL = 1,
	VER_RPRX_MATERIAL = 2,

	VER_CURRENT_PLUS_ONE,
	VER_CURRENT = VER_CURRENT_PLUS_ONE - 1
};

// Copy value of srcAttr to dstAttr
bool CopyAttribute(MFnDependencyNode& node, const MObject& srcAttr, const MObject& dstAttr, bool onlyNonEmpty = false)
{
	// Find plugs for both attributes
	MStatus status;
	MPlug src = node.findPlug(srcAttr, &status);
	CHECK_MSTATUS(status);
	MPlug dst = node.findPlug(dstAttr, &status);
	CHECK_MSTATUS(status);

	MPlugArray connections;
	src.connectedTo(connections, true, false);

	if (onlyNonEmpty)
	{
		if (connections.length() == 0)
			return false;
	}

	// Copy non-network value
	MDataHandle data = dst.asMDataHandle();
	data.copy(src.asMDataHandle());
	dst.setMDataHandle(data);

	// Copy network value
	if (connections.length())
	{
		MDGModifier modifier;
		for (unsigned int i = 0; i < connections.length(); i++)
		{
			DebugPrint("Connecting %s to %s", connections[i].name().asChar(), dst.name().asChar());
			modifier.connect(connections[i], dst);
			modifier.doIt();
		}
	}

	return true;
}

void BreakConnections(MFnDependencyNode& node, const MObject& attr)
{
	MStatus status;
	MPlug plug = node.findPlug(attr, &status);
	CHECK_MSTATUS(status);

	MPlugArray connections;
	plug.connectedTo(connections, true, false);

	if (connections.length())
	{
		MDGModifier modifier;
		for (unsigned int i = 0; i < connections.length(); i++)
		{
			modifier.disconnect(connections[i], plug);
			modifier.doIt();
		}
	}
}

void FireMaya::StandardMaterial::UpgradeMaterial()
{
#if USE_RPRX
	MFnDependencyNode shaderNode(thisMObject());

	MPlug plug = shaderNode.findPlug(Attribute::version);
	int version = plug.asInt();
	if (version < VER_CURRENT)
	{
		LogPrint("UpgradeMaterial: %s from ver %d", shaderNode.name().asChar(), version);
		//		CopyAttribute(shaderNode, Attribute::diffuseColor, Attribute::reflectionColor); -- this is for test: diffuse color will be copied to reflection color

		// Old shader model: Reflection Roughness X | Y, new shader moder: Reflection Roughness
		CopyAttribute(shaderNode, Attribute::reflectionRoughnessX, Attribute::reflectionRoughness);

		// Upgrade normal maps. Old material model had 4 normal maps, new one - only 1
		bool hasNormal = true;
		if (!CopyAttribute(shaderNode, Attribute::diffuseBaseNormal, Attribute::normalMap, true))
		{
			if (!CopyAttribute(shaderNode, Attribute::reflectionNormal, Attribute::normalMap, true))
			{
				if (!CopyAttribute(shaderNode, Attribute::clearCoatNormal, Attribute::normalMap, true))
				{
					if (!CopyAttribute(shaderNode, Attribute::refractionNormal, Attribute::normalMap, true))
					{
						hasNormal = false;
					}
				}
			}
		}
		// Enable normal map if it is connected
		if (hasNormal)
		{
			shaderNode.findPlug(Attribute::normalMapEnable).setBool(true);
		}

		BreakConnections(shaderNode, Attribute::reflectionRoughnessX);
		BreakConnections(shaderNode, Attribute::diffuseBaseNormal);
		BreakConnections(shaderNode, Attribute::reflectionNormal);
		BreakConnections(shaderNode, Attribute::clearCoatNormal);
		BreakConnections(shaderNode, Attribute::refractionNormal);
	}
#endif
}

// We're hooking 'shouldSave' to force new materials to be saved with VER_CURRENT. Without that, materials
// which were created during current session will be saved with default value of Attribute::version, which
// is set to minimal value for correct handling of older unversioned materials.
MStatus FireMaya::StandardMaterial::shouldSave(const MPlug& plug, bool& isSaving)
{
	if (plug.attribute() == Attribute::version)
	{
		MFnDependencyNode shaderNode(thisMObject());
		MPlug plug2 = shaderNode.findPlug(Attribute::version);
		CHECK_MSTATUS(plug2.setInt(VER_CURRENT));
	}
	// CaLL default implementation
	return MPxNode::shouldSave(plug, isSaving);
}

// creates an instance of the node
void* FireMaya::StandardMaterial::creator()
{
	return new StandardMaterial;
}

// initializes attribute information
// call by MAYA when this plug-in was loaded.
//
MStatus FireMaya::StandardMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

#if USE_RPRX
#define DEPRECATED_PARAM(attr) \
	CHECK_MSTATUS(attr.setCached(false)); \
	CHECK_MSTATUS(attr.setStorable(false)); \
	CHECK_MSTATUS(attr.setHidden(true));
#else
#define DEPRECATED_PARAM(attr)
#endif

#define SET_SOFTMINMAX(attr, min, max) \
	CHECK_MSTATUS(attr.setSoftMin(min)); \
	CHECK_MSTATUS(attr.setSoftMax(max));

#define SET_MINMAX(attr, min, max) \
	CHECK_MSTATUS(attr.setMin(min)); \
	CHECK_MSTATUS(attr.setMax(max));

#if USE_RPRX
	// Create version attribute. Set default value to VER_INITIAL for correct processing
	// of old un-versioned nodes.
	Attribute::version = nAttr.create("materialVersion", "mtlver", MFnNumericData::kInt, VER_INITIAL);
	CHECK_MSTATUS(nAttr.setCached(false));
	CHECK_MSTATUS(nAttr.setHidden(true));
	CHECK_MSTATUS(addAttribute(Attribute::version));
#endif

	// Diffuse
#if USE_RPRX
	Attribute::diffuseEnable = nAttr.create("diffuse", "dif", MFnNumericData::kBoolean, 1);
	MAKE_INPUT_CONST(nAttr);
#endif

	Attribute::diffuseColor = nAttr.createColor("diffuseColor", "dc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.644f, 0.644f, 0.644f));

#if USE_RPRX
	Attribute::diffuseWeight = nAttr.create("diffuseWeight", "dw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	///	CHECK_MSTATUS(nAttr.setDefault(1.0));
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::diffuseRoughness = nAttr.create("diffuseRoughness", "dr", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	///	CHECK_MSTATUS(nAttr.setDefault(1.0));
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);
#endif

	// Reflection
	Attribute::reflectionEnable = nAttr.create("reflections", "gr", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::reflectionColor = nAttr.createColor("reflectColor", "grc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

#if USE_RPRX
	Attribute::reflectionWeight = nAttr.create("reflectWeight", "rw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::reflectionRoughness = nAttr.create("reflectRoughness", "rr", MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);

	Attribute::reflectionAnisotropy = nAttr.create("reflectAnisotropy", "ra", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, -1.0, 1.0);

	Attribute::reflectionAnisotropyRotation = nAttr.create("reflectAnisotropyRotation", "rar", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT_CONST(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);
#endif

#if !USE_RPRX
	Attribute::reflectionRotation = nAttr.create("reflectRotation", "grr", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMin(0.0);
#endif

	Attribute::reflectionRoughnessX = nAttr.create("reflectRoughnessX", "grrx", MFnNumericData::kFloat, 0.1);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);
	DEPRECATED_PARAM(nAttr);

#if USE_RPRX
	Attribute::reflectionMetalMaterial = nAttr.create("reflectMetalMaterial", "rm", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::reflectionMetalness = nAttr.create("reflectMetalness", "rmet", MFnNumericData::kFloat, 1);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);
#endif

	Attribute::reflectionIOR = nAttr.create("reflectIOR", "grior", MFnNumericData::kFloat, 1.5);
	MAKE_INPUT_CONST(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 2.0);

#if !USE_RPRX
	Attribute::reflectionRoughnessY = nAttr.create("reflectRoughnessY", "grry", MFnNumericData::kFloat, 0.1);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);
#endif

	// Coating
	Attribute::clearCoatEnable = nAttr.create("clearCoat", "cc", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::clearCoatColor = nAttr.createColor("coatColor", "ccc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

#if USE_RPRX
	/// TODO
	Attribute::clearCoatWeight = nAttr.create("coatWeight", "ccw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::clearCoatRoughness = nAttr.create("coatRoughness", "ccr", MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);

	Attribute::clearCoatMetalMaterial = nAttr.create("coatMetalMaterial", "ccm", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::clearCoatMetalness = nAttr.create("coatMetalness", "ccmet", MFnNumericData::kFloat, 1);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);
#endif

	Attribute::clearCoatIOR = nAttr.create("coatIOR", "ccior", MFnNumericData::kFloat, 1.5);
	MAKE_INPUT_CONST(nAttr);
	SET_SOFTMINMAX(nAttr, 0.1, 2.0);

	// Refraction
#if USE_RPRX
	Attribute::refractionEnable = nAttr.create("refraction", "ref", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);
#endif

	Attribute::refractionColor = nAttr.createColor("refractColor", "refc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::refractionWeight = nAttr.create("refractWeight", "refl", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::refractionRoughness = nAttr.create("refractRoughness", "refr", MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);

	Attribute::refractionIOR = nAttr.create("refractIOR", "refior", MFnNumericData::kFloat, 1.5);
	MAKE_INPUT_CONST(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 2.0);

#if USE_RPRX
	Attribute::refractionLinkToReflection = nAttr.create("refractLinkToReflect", "reflink", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::refractionThinSurface = nAttr.create("refractThinSurface", "refth", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);
#endif

	// Emissive
#if USE_RPRX
	Attribute::emissiveEnable = nAttr.create("emissive", "em", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::emissiveColor = nAttr.createColor("emissiveColor", "emc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::emissiveWeight = nAttr.create("emissiveWeight", "emw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);

	Attribute::emissiveDoubleSided = nAttr.create("emissiveDoubleSided", "emds", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);
#endif

	// Material parameters
	Attribute::transparencyLevel = nAttr.create("transparencyLevel", "trl", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 1.0);

	Attribute::displacementMap = nAttr.createColor("displacementMap", "disp");
	MAKE_INPUT(nAttr);

#if USE_RPRX
	Attribute::normalMap = nAttr.createColor("normalMap", "nm");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::transparencyEnable = nAttr.create("transparencyEnable", "et", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::displacementEnable = nAttr.create("displacementEnable", "en", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::normalMapEnable = nAttr.create("normalMapEnable", "enm", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

#endif

#if !USE_RPRX
	Attribute::transparencyColor = nAttr.createColor("transparencyColor", "trc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));
#endif

	// Subsurface layer
#if USE_RPRX
	Attribute::sssEnable = nAttr.create("sssEnable", "enss", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::sssColor = nAttr.createColor("sssColor", "sssc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::sssWeight = nAttr.create("sssWeight", "sssw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::sssUseDiffuseColor = nAttr.create("sssUseDiffuseColor", "sssdif", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);
#endif

	Attribute::volumeScatter = nAttr.createColor("volumeScatter", "vs");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::volumeTransmission = nAttr.createColor("volumeTransmission", "vt");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::volumeDensity = nAttr.create("volumeDensity", "vd", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, 0.0, 10.0);

	Attribute::volumeScatteringDirection = nAttr.create("scatteringDirection", "vsd", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	SET_SOFTMINMAX(nAttr, -1.0, 1.0);

	Attribute::volumeMultipleScattering = nAttr.create("multipleScattering", "vms", MFnNumericData::kBoolean, true);
	MAKE_INPUT_CONST(nAttr);

#if !USE_RPRX
	Attribute::volumeEnable = nAttr.create("enableVolume", "v", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::volumeEmission = nAttr.createColor("volumeEmission", "ve");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.0f, 0.0f, 0.0f));
#endif

	Attribute::diffuseBaseNormal = nAttr.createPoint("diffuseNormal", "nmap");
	MAKE_INPUT(nAttr);
	DEPRECATED_PARAM(nAttr);

	Attribute::reflectionNormal = nAttr.createPoint("reflectNormal", "grnmap");
	MAKE_INPUT(nAttr);
	DEPRECATED_PARAM(nAttr);

	Attribute::clearCoatNormal = nAttr.createPoint("coatNormal", "ccnmap");
	MAKE_INPUT(nAttr);
	DEPRECATED_PARAM(nAttr);

	Attribute::refractionNormal = nAttr.createPoint("refNormal", "refnmap");
	MAKE_INPUT(nAttr);
	DEPRECATED_PARAM(nAttr);

	// output color
	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	// output transparency
	Attribute::outputAlpha = nAttr.createColor("outTransparency", "ot");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::output));
	CHECK_MSTATUS(addAttribute(Attribute::outputAlpha));

	// Register attribute and make it affecting output color and alpha
#define ADD_ATTRIBUTE(attr) \
	CHECK_MSTATUS(addAttribute(attr)); \
	CHECK_MSTATUS(attributeAffects(attr, Attribute::output)); \
	CHECK_MSTATUS(attributeAffects(attr, Attribute::outputAlpha));

#if USE_RPRX
	ADD_ATTRIBUTE(Attribute::diffuseEnable);
#endif
	ADD_ATTRIBUTE(Attribute::diffuseColor);
#if USE_RPRX
	ADD_ATTRIBUTE(Attribute::diffuseWeight);
	ADD_ATTRIBUTE(Attribute::diffuseRoughness);
#endif

	ADD_ATTRIBUTE(Attribute::reflectionEnable);
	ADD_ATTRIBUTE(Attribute::reflectionColor);
#if USE_RPRX
	ADD_ATTRIBUTE(Attribute::reflectionWeight);
	ADD_ATTRIBUTE(Attribute::reflectionRoughness);
	ADD_ATTRIBUTE(Attribute::reflectionAnisotropy);
	ADD_ATTRIBUTE(Attribute::reflectionAnisotropyRotation);
	ADD_ATTRIBUTE(Attribute::reflectionMetalMaterial);
	ADD_ATTRIBUTE(Attribute::reflectionMetalness);
#endif
	ADD_ATTRIBUTE(Attribute::reflectionIOR);
	ADD_ATTRIBUTE(Attribute::reflectionRoughnessX);
#if !USE_RPRX
	ADD_ATTRIBUTE(Attribute::reflectionRotation);
	ADD_ATTRIBUTE(Attribute::reflectionRoughnessY);
#endif

	ADD_ATTRIBUTE(Attribute::clearCoatEnable);
	ADD_ATTRIBUTE(Attribute::clearCoatColor);
	ADD_ATTRIBUTE(Attribute::clearCoatIOR);
#if USE_RPRX
	ADD_ATTRIBUTE(Attribute::clearCoatWeight);
	ADD_ATTRIBUTE(Attribute::clearCoatRoughness);
	ADD_ATTRIBUTE(Attribute::clearCoatMetalMaterial);
	ADD_ATTRIBUTE(Attribute::clearCoatMetalness);
#endif

#if USE_RPRX
	ADD_ATTRIBUTE(Attribute::refractionEnable);
#endif
	ADD_ATTRIBUTE(Attribute::refractionColor);
	ADD_ATTRIBUTE(Attribute::refractionWeight);
	ADD_ATTRIBUTE(Attribute::refractionRoughness);
	ADD_ATTRIBUTE(Attribute::refractionIOR);
#if USE_RPRX
	ADD_ATTRIBUTE(Attribute::refractionLinkToReflection);
	ADD_ATTRIBUTE(Attribute::refractionThinSurface);
#endif

#if USE_RPRX
	ADD_ATTRIBUTE(Attribute::emissiveEnable);
	ADD_ATTRIBUTE(Attribute::emissiveColor);
	ADD_ATTRIBUTE(Attribute::emissiveWeight);
	ADD_ATTRIBUTE(Attribute::emissiveDoubleSided);
#endif

	ADD_ATTRIBUTE(Attribute::transparencyLevel);
	ADD_ATTRIBUTE(Attribute::displacementMap);
#if USE_RPRX
	ADD_ATTRIBUTE(Attribute::normalMap);
	ADD_ATTRIBUTE(Attribute::normalMapEnable);

	ADD_ATTRIBUTE(Attribute::transparencyEnable);
	ADD_ATTRIBUTE(Attribute::displacementEnable);
#endif
#if !USE_RPRX
	ADD_ATTRIBUTE(Attribute::transparencyColor);
#endif

#if USE_RPRX
	ADD_ATTRIBUTE(Attribute::sssEnable);
	ADD_ATTRIBUTE(Attribute::sssUseDiffuseColor);
	ADD_ATTRIBUTE(Attribute::sssColor);
	ADD_ATTRIBUTE(Attribute::sssWeight);
#endif
	ADD_ATTRIBUTE(Attribute::volumeScatter);
	ADD_ATTRIBUTE(Attribute::volumeTransmission);
	ADD_ATTRIBUTE(Attribute::volumeDensity);
	ADD_ATTRIBUTE(Attribute::volumeScatteringDirection);
	ADD_ATTRIBUTE(Attribute::volumeMultipleScattering);
#if !USE_RPRX
	ADD_ATTRIBUTE(Attribute::volumeEnable);
	ADD_ATTRIBUTE(Attribute::volumeEmission);
#endif

	ADD_ATTRIBUTE(Attribute::diffuseBaseNormal);
	ADD_ATTRIBUTE(Attribute::reflectionNormal);
	ADD_ATTRIBUTE(Attribute::clearCoatNormal);
	ADD_ATTRIBUTE(Attribute::refractionNormal);

	return MS::kSuccess;
}

MStatus FireMaya::StandardMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		MFloatVector& surfaceColor = block.inputValue(Attribute::diffuseColor).asFloatVector();

		// set output color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = surfaceColor;
		outColorHandle.setClean();
		block.setClean(plug);
	}
	else if ((plug == Attribute::outputAlpha) || (plug.parent() == Attribute::outputAlpha))
	{
		MFloatVector tr(1.0, 1.0, 1.0);

		// set output color attribute
		MDataHandle outTransHandle = block.outputValue(Attribute::outputAlpha);
		MFloatVector& outTrans = outTransHandle.asFloatVector();
		outTrans = tr;
		block.setClean(plug);
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}

frw::Shader FireMaya::StandardMaterial::GetShader(Scope& scope)
{
#if USE_RPRX
	frw::Shader material(scope.MaterialSystem(), scope.Context(), RPRX_MATERIAL_UBER);
	MFnDependencyNode shaderNode(thisMObject());

#define GET_BOOL(_attrib_) \
	shaderNode.findPlug(Attribute::_attrib_).asBool()

#define GET_VALUE(_attrib_) \
	scope.GetValue(shaderNode.findPlug(Attribute::_attrib_))

#define SET_RPRX_VALUE(_param_, _attrib_) \
	material.xSetValue(_param_, GET_VALUE(_attrib_));

	// Diffuse
	if (GET_BOOL(diffuseEnable))
	{
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_DIFFUSE_COLOR, diffuseColor);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_DIFFUSE_WEIGHT, diffuseWeight);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_DIFFUSE_ROUGHNESS, diffuseRoughness);
	}
	else
	{
		material.xSetParameterF(RPRX_UBER_MATERIAL_DIFFUSE_WEIGHT, 0, 0, 0, 0);
	}

	// Reflection
	if (GET_BOOL(reflectionEnable))
	{
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFLECTION_COLOR, reflectionColor);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFLECTION_WEIGHT, reflectionWeight);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFLECTION_ROUGHNESS, reflectionRoughness);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFLECTION_ANISOTROPY, reflectionAnisotropy);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFLECTION_ANISOTROPY_ROTATION, reflectionAnisotropyRotation);
		// Metalness
		if (GET_BOOL(reflectionMetalMaterial))
		{
			// metallic material
			material.xSetParameterU(RPRX_UBER_MATERIAL_REFLECTION_MODE, RPRX_UBER_MATERIAL_REFLECTION_MODE_METALNESS);
			SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFLECTION_METALNESS, reflectionMetalness);
		}
		else
		{
			// PBR material
			material.xSetParameterU(RPRX_UBER_MATERIAL_REFLECTION_MODE, RPRX_UBER_MATERIAL_REFLECTION_MODE_PBR);
			SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFLECTION_IOR, reflectionIOR);
		}
	}
	else
	{
		material.xSetParameterF(RPRX_UBER_MATERIAL_REFLECTION_WEIGHT, 0, 0, 0, 0);
	}

	// Coating
	if (GET_BOOL(clearCoatEnable))
	{
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_COATING_COLOR, clearCoatColor);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_COATING_WEIGHT, clearCoatWeight);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_COATING_ROUGHNESS, clearCoatRoughness);
		// Metalness
		if (GET_BOOL(clearCoatMetalMaterial))
		{
			// metallic material
			material.xSetParameterU(RPRX_UBER_MATERIAL_COATING_MODE, RPRX_UBER_MATERIAL_COATING_MODE_METALNESS);
			SET_RPRX_VALUE(RPRX_UBER_MATERIAL_COATING_METALNESS, clearCoatMetalness);
		}
		else
		{
			// PBR material
			material.xSetParameterU(RPRX_UBER_MATERIAL_COATING_MODE, RPRX_UBER_MATERIAL_COATING_MODE_PBR);
			SET_RPRX_VALUE(RPRX_UBER_MATERIAL_COATING_IOR, clearCoatIOR);
		}
	}
	else
	{
		material.xSetParameterF(RPRX_UBER_MATERIAL_COATING_WEIGHT, 0, 0, 0, 0);
	}

	// Refraction
	if (GET_BOOL(refractionEnable))
	{
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFRACTION_COLOR, refractionColor);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFRACTION_WEIGHT, refractionWeight);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFRACTION_ROUGHNESS, refractionRoughness);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_REFRACTION_IOR, refractionIOR);
		bool bThinSurface = GET_BOOL(refractionThinSurface);
		bool bLinkedIOR = GET_BOOL(refractionLinkToReflection);
		// prevent crash in RPR (1.258) - "linked IOR" doesn't work when reflection mode set to "metallic"
		if (GET_BOOL(reflectionMetalMaterial) || !GET_BOOL(reflectionEnable))
			bLinkedIOR = false;
		material.xSetParameterU(RPRX_UBER_MATERIAL_REFRACTION_IOR_MODE, bLinkedIOR ? RPRX_UBER_MATERIAL_REFRACTION_MODE_LINKED : RPRX_UBER_MATERIAL_REFRACTION_MODE_SEPARATE);
		material.xSetParameterU(RPRX_UBER_MATERIAL_REFRACTION_THIN_SURFACE, bThinSurface ? RPR_TRUE : RPR_FALSE);
	}
	else
	{
		material.xSetParameterF(RPRX_UBER_MATERIAL_REFRACTION_WEIGHT, 0, 0, 0, 0);
	}

	// Emissive
	if (GET_BOOL(emissiveEnable))
	{
		frw::Value valueEmissiveWeight = scope.GetValue(shaderNode.findPlug(Attribute::emissiveWeight));
		material.xSetValue(RPRX_UBER_MATERIAL_EMISSION_WEIGHT, valueEmissiveWeight);

		frw::Value valueEmissiveColor = scope.GetValue(shaderNode.findPlug(Attribute::emissiveColor));

		const frw::MaterialSystem ms = valueEmissiveColor.GetMaterialSystem();
		valueEmissiveColor = ms.ValueMul(valueEmissiveColor, valueEmissiveWeight);
		material.xSetValue(RPRX_UBER_MATERIAL_EMISSION_COLOR, valueEmissiveColor);

		bool bDoubleSided = GET_BOOL(emissiveDoubleSided);
		material.xSetParameterU(RPRX_UBER_MATERIAL_EMISSION_MODE, bDoubleSided ? RPRX_UBER_MATERIAL_EMISSION_MODE_DOUBLESIDED : RPRX_UBER_MATERIAL_EMISSION_MODE_SINGLESIDED);
	}
	else
	{
		material.xSetParameterF(RPRX_UBER_MATERIAL_EMISSION_WEIGHT, 0, 0, 0, 0);
	}

	// Subsurface
	if (GET_BOOL(sssEnable))
	{
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_SSS_WEIGHT, sssWeight);
		if (GET_BOOL(sssUseDiffuseColor))
		{
			SET_RPRX_VALUE(RPRX_UBER_MATERIAL_SSS_SUBSURFACE_COLOR, diffuseColor);
		}
		else
		{
			SET_RPRX_VALUE(RPRX_UBER_MATERIAL_SSS_SUBSURFACE_COLOR, sssColor);
		}
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_SSS_WEIGHT, sssWeight);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_SSS_ABSORPTION_COLOR, volumeTransmission);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_SSS_SCATTER_COLOR, volumeScatter);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_SSS_ABSORPTION_DISTANCE, volumeDensity);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_SSS_SCATTER_DISTANCE, volumeDensity);
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_SSS_SCATTER_DIRECTION, volumeScatteringDirection);
		material.xSetParameterU(RPRX_UBER_MATERIAL_SSS_MULTISCATTER, GET_BOOL(volumeMultipleScattering) ? RPR_TRUE : RPR_FALSE);
	}
	else
	{
		material.xSetParameterF(RPRX_UBER_MATERIAL_SSS_WEIGHT, 0, 0, 0, 0);
	}

	// Material attributes
	if (GET_BOOL(transparencyEnable))
	{
		SET_RPRX_VALUE(RPRX_UBER_MATERIAL_TRANSPARENCY, transparencyLevel);
	}
	if (GET_BOOL(normalMapEnable))
	{
		frw::Value value = GET_VALUE(normalMap);
		int type = value.GetNodeType();
		if (type == frw::ValueTypeNormalMap || type == frw::ValueTypeBumpMap)
		{
			material.xSetValue(RPRX_UBER_MATERIAL_NORMAL, value);
		}
		else if (type >= 0)
		{
			ErrorPrint("%s NormalMap: invalid node type %d\n", shaderNode.name().asChar(), value.GetNodeType());
		}
	}

	// Special code for displacement map. We're using GetDisplacementNode() function which is called twice:
	// from this function, and from FireRenderMesh::setupDisplacement(). This is done because RPRX UberMaterial
	// doesn't have capabilities to set any displacement parameters except map image, so we're setting other
	// parameters from FireRenderMesh. If we'll skip setting RPRX_UBER_MATERIAL_DISPLACEMENT parameter here,
	// RPRX will reset displacement map in some unpredicted cases.
	MObject displacementNode = GetDisplacementNode();
	if (displacementNode != MObject::kNullObj)
	{
		MFnDependencyNode dispShaderNode(displacementNode);
		FireMaya::Displacement* displacement = dynamic_cast<FireMaya::Displacement*>(dispShaderNode.userNode());
		if (displacement)
		{
			float minHeight, maxHeight, creaseWeight;
			int subdivision, boundary;
			frw::Value mapValue;

			bool haveDisplacement = displacement->getValues(mapValue, scope, minHeight, maxHeight, subdivision, creaseWeight, boundary);
			if (haveDisplacement)
			{
				material.xSetValue(RPRX_UBER_MATERIAL_DISPLACEMENT, mapValue);
			}
		}
	}

#else
	auto ms = scope.MaterialSystem();

	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader material = frw::Shader(ms, frw::ShaderTypeStandard);

	// DIFFUSE
	{
		material.SetValue("diffuse.color", scope.GetValue(shaderNode.findPlug(Attribute::diffuseColor)));
		material.SetValue("diffuse.normal", scope.GetConnectedValue(shaderNode.findPlug(Attribute::diffuseBaseNormal)));
	}

	// REFLECTIONS (uses microfacet for now, will use ward)
	{
		if (!scope.GetValue(shaderNode.findPlug(Attribute::reflectionEnable)).GetBool())
			material.SetValue("weights.glossy2diffuse", frw::Value(0.f));
		else
		{
			material.SetValue("glossy.color", scope.GetValue(shaderNode.findPlug(Attribute::reflectionColor)));

			frw::Value roughx = scope.GetValue(shaderNode.findPlug(Attribute::reflectionRoughnessX));
			material.SetValue("glossy.roughness_x", ms.ValueAdd(roughx, 0.000001f)); // antonio: work-around

			frw::Value roughy = scope.GetValue(shaderNode.findPlug(Attribute::reflectionRoughnessY));
			material.SetValue("glossy.roughness_y", ms.ValueAdd(roughy, 0.000001f)); // antonio: work-around

			material.SetValue("glossy.normal", scope.GetConnectedValue(shaderNode.findPlug(Attribute::reflectionNormal)));

			material.SetValue("weights.glossy2diffuse", ms.ValueFresnel(scope.GetValue(shaderNode.findPlug(Attribute::reflectionIOR))));
		}
	}

	// CLEARCOAT (specular reflections)
	{
		if (!scope.GetValue(shaderNode.findPlug(Attribute::clearCoatEnable)).GetBool())
			material.SetValue("weights.clearcoat2glossy", frw::Value(0.f));
		else
		{
			material.SetValue("clearcoat.color", scope.GetValue(shaderNode.findPlug(Attribute::clearCoatColor)));
			material.SetValue("clearcoat.normal", scope.GetConnectedValue(shaderNode.findPlug(Attribute::clearCoatNormal)));
			material.SetValue("weights.clearcoat2glossy", ms.ValueFresnel(scope.GetValue(shaderNode.findPlug(Attribute::clearCoatIOR))));
		}
	}

	// REFRACTION
	{
		material.SetValue("refraction.color", scope.GetValue(shaderNode.findPlug(Attribute::refractionColor)));

		material.SetValue("refraction.normal", scope.GetConnectedValue(shaderNode.findPlug(Attribute::refractionNormal)));

		material.SetValue("refraction.ior", frw::Value(scope.GetValue(shaderNode.findPlug(Attribute::refractionIOR))));

		material.SetValue("refraction.roughness", scope.GetValue(shaderNode.findPlug(Attribute::refractionRoughness)));

		frw::Value weight = scope.GetValue(shaderNode.findPlug(Attribute::refractionWeight));
		weight = ms.ValueSub(1.0, weight);
		material.SetValue("weights.diffuse2refraction", weight);
	}

	// TRANSPARENCY
	{
		material.SetValue("transparency.color", scope.GetValue(shaderNode.findPlug(Attribute::transparencyColor)));

		frw::Value weight = scope.GetValue(shaderNode.findPlug(Attribute::transparencyLevel));
		material.SetValue("weights.transparency", weight);
	}
#endif
	return material;
}

frw::Shader FireMaya::StandardMaterial::GetVolumeShader(Scope& scope)
{
#if !USE_RPRX
	auto ms = scope.MaterialSystem();
	MFnDependencyNode shaderNode(thisMObject());

	if (scope.GetValue(shaderNode.findPlug(Attribute::volumeEnable)).GetBool())
	{
		frw::Shader material = frw::Shader(ms, frw::ShaderTypeVolume);
		auto scatterColor = scope.GetValue(shaderNode.findPlug(Attribute::volumeScatter));
		auto transmissionColor = scope.GetValue(shaderNode.findPlug(Attribute::volumeTransmission));
		auto emissionColor = scope.GetValue(shaderNode.findPlug(Attribute::volumeEmission));
		auto k = shaderNode.findPlug(Attribute::volumeDensity).asFloat();
		auto scatteringDirection = shaderNode.findPlug(Attribute::volumeScatteringDirection).asFloat();
		auto multiScatter = shaderNode.findPlug(Attribute::volumeMultipleScattering).asBool();

		// scattering
		material.SetValue("sigmas", scatterColor * k);

		// absorption
		material.SetValue("sigmaa", (1 - transmissionColor) * k);

		// emission
		material.SetValue("emission", emissionColor * k);

		// phase and multi on/off
		material.SetValue("g", scatteringDirection);
		material.SetValue("multiscatter", multiScatter ? 1.f : 0.f);
		return material;
	}
#endif
	return frw::Shader();
}

MObject FireMaya::StandardMaterial::GetDisplacementNode()
{
	MFnDependencyNode shaderNode(thisMObject());

	if (!GET_BOOL(displacementEnable))
		return MObject::kNullObj;

	MPlug plug = shaderNode.findPlug(Attribute::displacementMap);
	if (plug.isNull())
		return MObject::kNullObj;

	MPlugArray shaderConnections;
	plug.connectedTo(shaderConnections, true, false);
	if (shaderConnections.length() == 0)
		return MObject::kNullObj;
	MObject node = shaderConnections[0].node();
	return node;
}