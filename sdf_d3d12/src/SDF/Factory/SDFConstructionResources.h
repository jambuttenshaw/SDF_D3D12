#pragma once

#include "Core.h"

#include "Renderer/Buffer/CounterResource.h"
#include "Renderer/Buffer/ReadbackBuffer.h"
#include "Renderer/Buffer/StructuredBuffer.h"
#include "Renderer/Buffer/UploadBuffer.h"

#include "Renderer/Hlsl/ComputeHlslCompat.h"
#include "SDF/SDFEditList.h"


class SDFConstructionResources
{
public:
	SDFConstructionResources(UINT brickCapacity);
	~SDFConstructionResources() = default;

	DISALLOW_COPY(SDFConstructionResources)
	DEFAULT_MOVE(SDFConstructionResources)

	void AllocateResources(const SDFEditList& editList, const Brick& initialBrick);
	inline void SwapBuffers() { m_CurrentReadBuffers = 1 - m_CurrentReadBuffers; }

	// Getters
	inline SDFEditBuffer& GetEditBuffer() { return m_EditBuffer; }

	inline StructuredBuffer<Brick>& GetReadBrickBuffer() { return m_BrickBuffers.at(m_CurrentReadBuffers); }
	inline StructuredBuffer<Brick>& GetWriteBrickBuffer() { return m_BrickBuffers.at(1 - m_CurrentReadBuffers); }

	inline CounterResource& GetReadCounter() { return m_SubBrickCounters.at(m_CurrentReadBuffers); }
	inline CounterResource& GetWriteCounter() { return m_SubBrickCounters.at(1 - m_CurrentReadBuffers); }

	inline DefaultBuffer& GetSubBrickCountBuffer() { return m_SubBrickCountBuffer; }
	inline DefaultBuffer& GetBlockPrefixSumsBuffer() { return m_BlockPrefixSumsBuffer; }
	inline DefaultBuffer& GetPrefixSumsBuffer() { return m_PrefixSumsBuffer; }

	inline UploadBuffer<Brick>& GetBrickUploadBuffer() { return m_BrickUpload; }
	inline UploadBuffer<UINT>& GetCounterUploadZeroBuffer() { return m_CounterUploadZero; }
	inline UploadBuffer<UINT>& GetCounterUploadOneBuffer() { return m_CounterUploadOne; }
	inline ReadbackBuffer<UINT>& GetCounterReadbackBuffer() { return m_CounterReadback; }
	
protected:
	bool m_Allocated = false;

	UINT m_BrickCapacity = 0;

	SDFEditBuffer m_EditBuffer;

	// Ping-pong buffers to contain the bricks
	// Which index is currently being read from
	UINT m_CurrentReadBuffers = 0;
	std::array<StructuredBuffer<Brick>, 2> m_BrickBuffers;
	std::array<CounterResource, 2> m_SubBrickCounters;

	// Buffers for calculating prefix sums
	DefaultBuffer m_SubBrickCountBuffer;			// Contains sub-brick counts for each brick
	DefaultBuffer m_BlockPrefixSumsBuffer;			// Contains the prefix sum of each entire block
	DefaultBuffer m_PrefixSumsBuffer;				// The final prefix sums output buffer

	// Utility buffers to set and read values
	UploadBuffer<Brick> m_BrickUpload;			// An upload buffer for bricks is required to send the initial brick to the GPU
	UploadBuffer<UINT32> m_CounterUploadZero;	// Used to set a counter to 0
	UploadBuffer<UINT32> m_CounterUploadOne;	// Used to set a counter to 1
	ReadbackBuffer<UINT32> m_CounterReadback;	// Used to read the value of a counter
};
