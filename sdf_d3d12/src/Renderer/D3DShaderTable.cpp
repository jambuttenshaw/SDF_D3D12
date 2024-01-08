#include "pch.h"
#include "D3DShaderTable.h"


D3DShaderRecord::D3DShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize) :
	m_ShaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
{
}

D3DShaderRecord::D3DShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize) :
	m_ShaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
	m_LocalRootArguments(pLocalRootArguments, localRootArgumentsSize)
{
}
void D3DShaderRecord::CopyTo(void* dest) const
{
	const auto destByte = static_cast<uint8_t*>(dest);

	if (!m_ShaderIdentifier.ptr)
	{
		LOG_WARN("Attempting to copy a null shader record");
		return;
	}

	memcpy(destByte, m_ShaderIdentifier.ptr, m_ShaderIdentifier.size);
	if (m_LocalRootArguments.ptr)
	{
		memcpy(destByte + m_ShaderIdentifier.size, m_LocalRootArguments.ptr, m_LocalRootArguments.size);
	}
}


D3DShaderTable::D3DShaderTable(ID3D12Device* device, UINT capacity, UINT recordSize)
	: m_Capacity(capacity)
{
	// make sure record size is aligned
	m_RecordSize = Align(recordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

	// calculate required size of buffer
	const UINT64 width = static_cast<UINT64>(m_Capacity) * m_RecordSize;

	// allocate buffer
	const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto desc = CD3DX12_RESOURCE_DESC::Buffer(width);
	THROW_IF_FAIL(device->CreateCommittedResource(
		&uploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_Resource)));

	// map resource
	THROW_IF_FAIL(m_Resource->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedByte)));
}


bool D3DShaderTable::AddRecord(const D3DShaderRecord& record)
{
	if (m_NumRecords >= m_Capacity)
		return false;

	record.CopyTo(m_MappedByte);
	m_MappedByte += m_RecordSize;

	return true;
}
