#include "pch.h"
#include "ShaderTable.h"

#include "D3DGraphicsContext.h"


///////////////////////
//// Shader Record ////
///////////////////////

ShaderRecord::ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize) :
	m_ShaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
{
}

ShaderRecord::ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize) :
	m_ShaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
	m_LocalRootArguments(pLocalRootArguments, localRootArgumentsSize)
{
}
void ShaderRecord::CopyTo(void* dest) const
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


//////////////////////
//// Shader Table ////
//////////////////////

ShaderTable::ShaderTable(ID3D12Device* device, UINT capacity, UINT recordSize, const wchar_t* name)
	: m_Capacity(capacity)
	, m_RecordBuffers(D3DGraphicsContext::GetBackBufferCount())
	, m_RecordBufferStaging(nullptr)
{
	m_RecordSize = Align(recordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

	// Allocate shader record buffers on gpu in upload heap

	const auto width = m_RecordSize * m_Capacity;
	const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto desc = CD3DX12_RESOURCE_DESC::Buffer(width);

	for (auto& buffer : m_RecordBuffers)
	{
		THROW_IF_FAIL(device->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer.Buffer)));
		buffer.Buffer->SetName(name);

		THROW_IF_FAIL(buffer.Buffer->Map(0, nullptr, reinterpret_cast<void**>(&buffer.MappedByte)));
	}

	// Allocate a staging buffer on cpu
	m_RecordBufferStaging = new UINT8[width];
}

ShaderTable::~ShaderTable()
{
	for (const auto& buffer : m_RecordBuffers)
	{
		buffer.Buffer->Unmap(0, nullptr);
	}

	delete[] m_RecordBufferStaging;
}

D3D12_GPU_VIRTUAL_ADDRESS ShaderTable::GetAddress() const
{
	const auto currentFrame = g_D3DGraphicsContext->GetCurrentBackBuffer();
	return m_RecordBuffers.at(currentFrame).Buffer->GetGPUVirtualAddress();
}

UINT ShaderTable::AddRecord(const ShaderRecord& record)
{
	if (m_NumRecords >= m_Capacity)
		return -1;

	m_FramesDirty = D3DGraphicsContext::GetBackBufferCount();

	record.CopyTo(m_RecordBufferStaging + static_cast<size_t>(m_NumRecords * m_RecordSize));
	return m_NumRecords++;
}

bool ShaderTable::UpdateRecord(UINT recordIndex, const ShaderRecord& record)
{
	if (recordIndex >= m_NumRecords)
		return false;

	m_FramesDirty = D3DGraphicsContext::GetBackBufferCount();

	record.CopyTo(m_RecordBufferStaging + static_cast<size_t>(recordIndex * m_RecordSize));
	return true;
}

void ShaderTable::CopyStagingToGPU()
{
	if (m_FramesDirty == 0)
		return;

	const auto currentFrame = g_D3DGraphicsContext->GetCurrentBackBuffer();
	memcpy(m_RecordBuffers.at(currentFrame).MappedByte, m_RecordBufferStaging, m_NumRecords * m_RecordSize);

	m_FramesDirty--;
}
