#include "pch.h"
#include "MemoryAllocator.h"

#include "Renderer/D3DGraphicsContext.h"


// DescriptorAllocation


DescriptorAllocation::DescriptorAllocation(DescriptorHeap* heap, UINT index, UINT count, bool cpuOnly)
	: m_Heap(heap)
	, m_Index(index)
	, m_Count(count)
	, m_CPUOnly(cpuOnly)
	, m_IsValid(true)
{
}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& other) noexcept
{
	m_Heap = std::move(other.m_Heap);
	m_Index = std::move(other.m_Index);
	m_Count = std::move(other.m_Count);

	m_CPUOnly = std::move(other.m_CPUOnly);

	m_IsValid = std::move(other.m_IsValid);

	other.Reset();
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& other) noexcept
{
	m_Heap = std::move(other.m_Heap);
	m_Index = std::move(other.m_Index);
	m_Count = std::move(other.m_Count);

	m_CPUOnly = std::move(other.m_CPUOnly);

	m_IsValid = std::move(other.m_IsValid);

	other.Reset();

	return *this;
}


D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetCPUHandle(UINT index) const
{
	ASSERT(m_IsValid, "Cannot get handle on an invalid allocation");
	ASSERT(index < m_Count, "Invalid descriptor index");

	const INT offset = static_cast<INT>(m_Index + index);
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_Heap->GetHeap()->GetCPUDescriptorHandleForHeapStart(), offset, m_Heap->GetDescriptorSize());
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetGPUHandle(UINT index) const
{
	ASSERT(m_IsValid, "Cannot get handle on an invalid allocation");
	ASSERT(!m_CPUOnly, "Cannot get GPU handle on a cpu-only descriptor");
	ASSERT(index < m_Count, "Invalid descriptor index");

	const INT offset = static_cast<INT>(m_Index + index);
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_Heap->GetHeap()->GetGPUDescriptorHandleForHeapStart(), offset, m_Heap->GetDescriptorSize());
}

ID3D12DescriptorHeap* DescriptorAllocation::GetHeap() const
{
	return m_Heap->GetHeap();
}

void DescriptorAllocation::Reset()
{
	m_Heap = nullptr;	
	m_Index = 0;						
	m_Count = 0;						

	m_CPUOnly = false;					

	m_IsValid = false;
}

void DescriptorAllocation::Free()
{
	if (m_IsValid && m_Heap)
	{
		m_Heap->Free(*this);
	}
}


// DescriptorHeap


DescriptorHeap::DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool cpuOnly, const wchar_t* resourceName)
	: m_Type(type)
	, m_Capacity(capacity)
	, m_CPUOnly(cpuOnly)
{
	// RTV and DSV heaps cannot be gpu visible
	if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
		m_CPUOnly = true;

	// TODO: Add assertions for capacity/type combos (sampler heaps have smaller max size)

	// Create the descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = m_Type;
	desc.NumDescriptors = m_Capacity;
	desc.Flags = m_CPUOnly ? D3D12_DESCRIPTOR_HEAP_FLAG_NONE : D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 1;

	THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));
	if (resourceName)
	{
		THROW_IF_FAIL(m_Heap->SetName(resourceName));
	}

	m_DescriptorIncrementSize = g_D3DGraphicsContext->GetDevice()->GetDescriptorHandleIncrementSize(m_Type);

	// Currently, the entire heap is free
	m_FreeBlocks.insert({ 0, m_Capacity });
	m_DeferredFrees.resize(g_D3DGraphicsContext->GetBackBufferCount());
}

DescriptorHeap::~DescriptorHeap()
{
	ASSERT(m_Count == 0, "Don't destroy allocators that still have memory allocated!!!");

	// Heap will automatically be released
	// GPU must not be using any of the allocations made

	// TODO: defer release of the heap (to make sure GPU is idle when this is released)
}

DescriptorAllocation DescriptorHeap::Allocate(UINT countToAlloc)
{
	ASSERT(countToAlloc > 0, "Invalid quantity of descriptors");

	if (m_Count + countToAlloc > m_Capacity)
	{
		LOG_ERROR("Descriptor allocation failed: no capacity in heap. Heap size: {0} Count to alloc: {1}", m_Capacity, countToAlloc);
		return {};
	}

	// Find the next free block
	auto freeBlockIt = m_FreeBlocks.begin();
	for (; freeBlockIt != m_FreeBlocks.end() && freeBlockIt->second < countToAlloc; ++freeBlockIt) {}
	if (freeBlockIt == m_FreeBlocks.end())
	{
		// No space in the heap
		LOG_ERROR("Descriptor allocation failed: no capacity in heap. Heap size: {0} Count to alloc: {1}", m_Capacity, countToAlloc);
		return {};
	}


	const UINT allocIndex = freeBlockIt->first;

	// Update the free blocks to reflect the changes
	UINT newFreeIndex = allocIndex + countToAlloc;
	UINT newFreeSize = freeBlockIt->second - countToAlloc;
	m_FreeBlocks.erase(freeBlockIt);
	// If there is still empty space insert it into the free blocks
	if (newFreeSize > 0)
	{
		m_FreeBlocks.insert(std::make_pair(newFreeIndex, newFreeSize));
	}

	m_Count += countToAlloc;

	return DescriptorAllocation{ this, allocIndex, countToAlloc, m_CPUOnly };
}

void DescriptorHeap::Free(DescriptorAllocation& allocation)
{
	if (!allocation.IsValid())
		return;

	// Make sure this heap contains this allocation
	ASSERT(allocation.GetHeap() == GetHeap(), "Trying to free an allocation from the wrong heap");

	m_DeferredFrees[g_D3DGraphicsContext->GetCurrentBackBuffer()].push_back(std::make_pair(allocation.GetIndex(), allocation.GetCount()));
	allocation.Reset();
}

void DescriptorHeap::ProcessDeferredFree(UINT frameIndex)
{
	// TODO: add validation

	if (m_DeferredFrees[frameIndex].empty())
		// No deferred frees to be made
		return;

	for (const auto& freedRange : m_DeferredFrees[frameIndex])
	{
		// Return freed allocation to the heap
		m_FreeBlocks.insert(freedRange);
		m_Count -= freedRange.second;
	}
	m_DeferredFrees[frameIndex].clear();

	// Combine adjacent freed blocks
	for (auto it = m_FreeBlocks.begin(); it != m_FreeBlocks.end();)
	{
		auto toMerge = m_FreeBlocks.find(it->first + it->second);
		if (toMerge != m_FreeBlocks.end())
		{
			// Contiguous block exists, merge into a single block
			it->second += toMerge->second;
			it = m_FreeBlocks.erase(toMerge);
		}
		else
		{
			++it;
		}
	}
}
