/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
	http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#pragma once

#include "FireRenderObjects.h"
#include "AlembicWrapper.hpp"

#include "Alembic/Abc/IArchive.h"

#include <vector>
#include <array>
#include <memory>
#include <map>
#include <sstream>
#include <functional>

class FireRenderGPUCache : public FireRenderNode
{
public:
	// Constructor
	FireRenderGPUCache(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderGPUCache(void);
	
	// Clear
	virtual void clear() override;

	// Register the callback
	//virtual void RegisterCallbacks() override;

	// transform attribute changed callback
	//virtual void OnNodeDirty() override;

	// node dirty
	//virtual void attributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug) override;
	virtual void Freshen() override;

	void Rebuild(void);

protected:
	void GetShapes(std::vector<frw::Shape>& outShapes);

	bool IsSelected(const MDagPath& dagPath) const;

	//virtual bool IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const;

	// Detach from the scene
	virtual void detachFromScene() override;

	// Attach to the scene
	virtual void attachToScene() override;

protected:
	std::vector<FrElement> elements;

	Alembic::Abc::IArchive m_archive;
	RPRAlembicWrapper::AlembicStorage m_storage;
	std::shared_ptr<RPRAlembicWrapper::AlembicScene> m_scene;
};



