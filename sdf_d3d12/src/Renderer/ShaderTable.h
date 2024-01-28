#pragma once
#include "Buffer/UploadBuffer.h"


using Microsoft::WRL::ComPtr;


class ShaderRecord
{
public:
	ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize);

	ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize);

	void CopyTo(void* dest) const;
		
private:
	struct PointerWithSize
	{
		void* ptr;
		UINT size;

		PointerWithSize()
			: ptr(nullptr), size(0) {}
		PointerWithSize(void* _ptr, UINT _size)
			: ptr(_ptr), size(_size) {}
	};

	PointerWithSize m_ShaderIdentifier;
	PointerWithSize m_LocalRootArguments;
};


class ShaderTable
{
public:
	ShaderTable(ID3D12Device* device, UINT capacity, UINT recordSize, const wchar_t* name);
	~ShaderTable();

	DISALLOW_COPY(ShaderTable)
	DEFAULT_MOVE(ShaderTable)

	inline UINT GetNumRecords() const { return m_NumRecords; }

	D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const;
	inline UINT64 GetSize() const { return static_cast<UINT64>(m_NumRecords) * m_RecordSize; }
	inline UINT64 GetStride() const { return m_RecordSize; }

	// Will copy the record (including its local arguments) into the staging shader table at the next free slot
	UINT AddRecord(const ShaderRecord& record);
	bool UpdateRecord(UINT recordIndex, const ShaderRecord& record);

	// Copies the staging buffer to the GPU
	void CopyStagingToGPU();

private:
	UINT64 m_RecordSize = -1;

	UINT m_NumRecords = 0;
	UINT m_Capacity = -1;

	// One upload buffer per frame buffer to contain the shader records
	struct ShaderRecordBuffer
	{
		ComPtr<ID3D12Resource> Buffer;
		UINT8* MappedByte;
	};
	std::vector<ShaderRecordBuffer> m_RecordBuffers;
	// A staging buffer to place shader records before they are copied to the current record buffer before rendering
	UINT8* m_RecordBufferStaging;

	UINT m_FramesDirty = 0;
};
