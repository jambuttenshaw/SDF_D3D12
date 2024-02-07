#pragma once

#include "SDFTypes.h"
#include "Renderer/Buffer/UploadBuffer.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"


class SDFEditList
{
public:
	SDFEditList(UINT maxEdits);
	~SDFEditList() = default;

	DEFAULT_COPY(SDFEditList)
	DEFAULT_MOVE(SDFEditList)

	void Reset();
	bool AddEdit(const SDFEdit& edit);

	// Getters
	inline UINT GetEditCount() const { return m_EditCount; }
	inline UINT GetMaxEdits() const { return m_MaxEdits; }

	inline const SDFEditData* GetEditData() const { return m_Edits.data(); }

private:
	SDFEditData BuildEditData(const SDFEdit& edit);

private:
	std::vector<SDFEditData> m_Edits;

	// Buffer capacity
	UINT m_MaxEdits = 0;
	UINT m_EditCount = 0;
};


// A representation of an edit list in GPU memory
class SDFEditBuffer
{
public:
	SDFEditBuffer() = default;
	~SDFEditBuffer() = default;

	DISALLOW_COPY(SDFEditBuffer);
	DEFAULT_MOVE(SDFEditBuffer);

	void Allocate(UINT maxEdits);
	void Populate(const SDFEditList& editList) const;

	inline UINT GetMaxEdits() const { return m_MaxEdits; }
	inline D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const { return m_EditsBuffer.GetAddressOfElement(0); }

private:
	UINT m_MaxEdits = 0;
	UploadBuffer<SDFEditData> m_EditsBuffer;

};
