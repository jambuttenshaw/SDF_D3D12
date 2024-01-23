#pragma once

#include "SDFTypes.h"
#include "Renderer/Buffer/UploadBuffer.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"


class SDFEditList
{
public:
	SDFEditList(UINT maxEdits);
	~SDFEditList() = default;

	DISALLOW_COPY(SDFEditList);
	DEFAULT_MOVE(SDFEditList);

	void Reset();
	bool AddEdit(const SDFEdit& edit);

	void CopyStagingToGPU() const;

	// Getters
	inline UINT GetEditCount() const { return static_cast<UINT>(m_EditsStaging.size()); }
	inline UINT GetMaxEdits() const { return m_MaxEdits; }

	inline D3D12_GPU_VIRTUAL_ADDRESS GetEditBufferAddress() const { return m_EditsBuffer.GetAddressOfElement(0); }

private:
	SDFEditData BuildEditData(const SDFEdit& edit);

private:
	// A staging buffer containing all the edits
	std::vector<SDFEditData> m_EditsStaging;

	// A structured buffer to contain the edits in GPU memory
	UploadBuffer<SDFEditData> m_EditsBuffer;

	// Buffer capacity
	UINT m_MaxEdits = 0;
};
