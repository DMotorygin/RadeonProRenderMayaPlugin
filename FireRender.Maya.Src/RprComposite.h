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

#include <memory>
#include <vector>
#include "RadeonProRender.h"
#include "frWrap.h"

/**
 * Class responsible for wrapping rpr_composite object.
 *
 * rpr_composite created in constructor. Exception thrown in case of error
 * rpr_composite destroyed in destructor.
 */
class RprComposite
{
public:
	RprComposite(rpr_context context, rpr_composite_type type);
	RprComposite();
	~RprComposite();
	operator rpr_composite () const;

	void SetInputC(const char *inputName, rpr_composite input);
	void SetInputC(const char *inputName, const RprComposite& input);
	void SetInputFb(const char *inputName, rpr_framebuffer input);
	void SetInput4f(const char *inputName, float r, float g, float b, float a);
	void SetInputOp(const char *inputName, rpr_material_node_arithmetic_operation op);
	void SetInput1U(const char *inputName, rpr_composite_type type);

	void SaveDependency(const std::unique_ptr<RprComposite>& fromTemporary);

private:
	rpr_composite mData = 0;
	rpr_context mContext = 0;

	std::vector<std::unique_ptr<RprComposite>> m_dependencies;
};

class CompositeWrapper
{
public:
	CompositeWrapper(const CompositeWrapper& other);
	CompositeWrapper(CompositeWrapper&& other);
	~CompositeWrapper();

	CompositeWrapper& operator= (CompositeWrapper&& other); // move assignment

	friend CompositeWrapper operator+ (const CompositeWrapper& w1, const CompositeWrapper& w2);
	friend CompositeWrapper operator+ (const CompositeWrapper&& w1, const CompositeWrapper& w2);
	friend CompositeWrapper operator+ (const CompositeWrapper& w1, const CompositeWrapper&& w2);
	friend CompositeWrapper operator+ (const CompositeWrapper&& w1, const CompositeWrapper&& w2);
	friend CompositeWrapper operator- (const CompositeWrapper& w1, const CompositeWrapper& w2);
	friend CompositeWrapper operator- (const CompositeWrapper&& w1, const CompositeWrapper& w2);
	friend CompositeWrapper operator- (const CompositeWrapper& w1, const CompositeWrapper&& w2);
	friend CompositeWrapper operator* (const CompositeWrapper& w1, const CompositeWrapper& w2);
	friend CompositeWrapper operator* (const CompositeWrapper&& w1, const CompositeWrapper& w2);
	friend CompositeWrapper operator* (const CompositeWrapper& w1, const CompositeWrapper&& w2);
	friend CompositeWrapper operator* (const CompositeWrapper&& w1, const CompositeWrapper&& w2);
	static CompositeWrapper min(const CompositeWrapper& first, const CompositeWrapper& second);

	CompositeWrapper(frw::Context& context, rpr_framebuffer frameBuffer);
	CompositeWrapper(frw::Context& context, const float val);
	CompositeWrapper(frw::Context& context, const float val1, const float val2, const float val3, const float val4);
	CompositeWrapper(frw::Context& context, const float val, const float val4);

	void Compute(frw::FrameBuffer& out);

	// use for debugging
	const RprComposite& GetCompoite(void);

protected:
	CompositeWrapper(rpr_context pContext, rpr_composite_type type);

protected:
	std::unique_ptr<RprComposite> m_composite;
	void* m_pContext;
};

