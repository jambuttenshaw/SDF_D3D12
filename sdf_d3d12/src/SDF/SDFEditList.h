#pragma once

#include "SDFTypes.h"
#include "Renderer/Buffer/UploadBuffer.h"
#include "Renderer/Hlsl/ComputeHlslCompat.h"


class SDFEditList
{
public:
	SDFEditList(UINT maxEdits, float evaluationRange = 2.0f);
	~SDFEditList() = default;

	DEFAULT_COPY(SDFEditList)
	DEFAULT_MOVE(SDFEditList)

	void Reset();
	bool AddEdit(const SDFEdit& edit);

	// Getters
	inline UINT GetEditCount() const { return m_EditCount; }
	inline UINT GetMaxEdits() const { return m_MaxEdits; }

	inline const SDFEditData* GetEditData() const { return m_Edits.data(); }

	inline float GetEvaluationRange() const { return m_EvaluationRange; }

private:
	SDFEditData BuildEditData(const SDFEdit& edit);

private:
	inline static constexpr UINT s_EditLimit = 1024;

	std::vector<SDFEditData> m_Edits;

	// Buffer capacity
	UINT m_MaxEdits = 0;
	UINT m_EditCount = 0;

	float m_EvaluationRange = 0.0f;
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
