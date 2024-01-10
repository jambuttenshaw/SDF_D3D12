#pragma once


using Microsoft::WRL::ComPtr;


class D3DShaderRecord
{
public:
	D3DShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize);

	D3DShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize);

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


class D3DShaderTable
{
public:
	D3DShaderTable(ID3D12Device* device, UINT capacity, UINT recordSize, const wchar_t* name);

	D3D12_GPU_VIRTUAL_ADDRESS GetAddress() const { return m_Resource->GetGPUVirtualAddress(); }
	UINT64 GetSize() const { return static_cast<UINT64>(m_NumRecords) * m_RecordSize; }
	UINT64 GetStride() const { return m_RecordSize; }

	// Will copy the record (including its local arguments) into the shader table buffer resource
	bool AddRecord(const D3DShaderRecord& record);

private:
	// An upload buffer to contain the shader records
	ComPtr<ID3D12Resource> m_Resource;

	UINT m_RecordSize = -1;

	UINT m_NumRecords = 0;
	UINT m_Capacity = -1;

	// points to the first free space in the buffer that a record could be copied into
	UINT8* m_MappedByte = nullptr;
};
