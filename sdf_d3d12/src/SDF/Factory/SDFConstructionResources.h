#pragma once

#include "Core.h"

#include "Renderer/Buffer/CounterResource.h"
#include "Renderer/Buffer/ReadbackBuffer.h"
#include "Renderer/Buffer/StructuredBuffer.h"
#include "Renderer/Buffer/UploadBuffer.h"

#include "Renderer/Hlsl/ComputeHlslCompat.h"
#include "SDF/SDFEditList.h"

// An encapsulation of all temporary resources required to perform an SDF object build
// These are encapsulated to make the construction pipeline neater and more readable
// It also makes multithreading/pipelining the construction much simpler
class SDFConstructionResources
{
public:
	SDFConstructionResources() = default;
	~SDFConstructionResources() = default;

	DISALLOW_COPY(SDFConstructionResources)
	DEFAULT_MOVE(SDFConstructionResources)

	// Don't call while a previous construction using this object is in flight!
	void AllocateResources(UINT brickCapacity, const SDFEditList& editList, float evalSpaceSize);
	void SwapBuffersAndRefineBrickSize();

	// Getters
	inline SDFEditBuffer& GetEditBuffer() { return m_EditBuffer; }

	inline BrickBuildParametersConstantBuffer& GetBrickBuildParams() { return m_BuildParamsCB; }
	inline BrickEvaluationConstantBuffer& GetBrickEvalParams() { return m_BrickEvalCB; }

	inline StructuredBuffer<Brick>& GetReadBrickBuffer() { return m_BrickBuffers.at(m_CurrentReadBuffers); }
	inline StructuredBuffer<Brick>& GetWriteBrickBuffer() { return m_BrickBuffers.at(1 - m_CurrentReadBuffers); }

	inline CounterResource& GetReadCounter() { return m_SubBrickCounters.at(m_CurrentReadBuffers); }
	inline CounterResource& GetWriteCounter() { return m_SubBrickCounters.at(1 - m_CurrentReadBuffers); }

	inline DefaultBuffer& GetSubBrickCountBuffer() { return m_SubBrickCountBuffer; }
	inline DefaultBuffer& GetBlockPrefixSumsBuffer() { return m_BlockPrefixSumsBuffer; }
	inline DefaultBuffer& GetBlockPrefixSumsOutputBuffer() { return m_BlockPrefixSumsOutputBuffer; }
	inline DefaultBuffer& GetPrefixSumsBuffer() { return m_PrefixSumsBuffer; }

	inline UploadBuffer<Brick>& GetBrickUploadBuffer() { return m_BrickUpload; }
	inline ReadbackBuffer<UINT>& GetCounterReadbackBuffer() { return m_CounterReadback; }

	inline DefaultBuffer& GetCommandBuffer() { return m_CommandBuffer; }

protected:
	UINT m_BrickCapacity = 0;
	bool m_Allocated = false;

	// Edits
	SDFEditBuffer m_EditBuffer;

	// Constant buffers
	BrickBuildParametersConstantBuffer m_BuildParamsCB;
	BrickEvaluationConstantBuffer m_BrickEvalCB;

	// Ping-pong buffers to contain the bricks
	// Which index is currently being read from
	UINT m_CurrentReadBuffers = 0;
	std::array<StructuredBuffer<Brick>, 2> m_BrickBuffers;
	std::array<CounterResource, 2> m_SubBrickCounters;

	// Buffers for calculating prefix sums
	DefaultBuffer m_SubBrickCountBuffer;			// Contains sub-brick counts for each brick
	DefaultBuffer m_BlockPrefixSumsBuffer;			// Contains the sums of each entire block
	DefaultBuffer m_BlockPrefixSumsOutputBuffer;	// Contains the prefix sum of the sums of each entire block
	DefaultBuffer m_PrefixSumsBuffer;				// The final prefix sums output buffer

	// Utility buffers to set and read values
	UploadBuffer<Brick> m_BrickUpload;			// An upload buffer for bricks is required to send the initial brick to the GPU
	ReadbackBuffer<UINT32> m_CounterReadback;	// Used to read the value of a counter

	// Command buffer for indirect dispatching
	// This will contain 4 arguments
	inline static constexpr UINT s_NumCommands = 4;
	DefaultBuffer m_CommandBuffer;
};
