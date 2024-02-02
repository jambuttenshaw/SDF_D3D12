#include "pch.h"
#include "D3DQueue.h"


D3DQueue::D3DQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, const wchar_t* name)
	: m_QueueType(type)
{
	// Create queue
	{
		D3D12_COMMAND_QUEUE_DESC desc;
		desc.Type = m_QueueType;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		THROW_IF_FAIL(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_CommandQueue)));
		if (name)
		{
			m_CommandQueue->SetName(name);
		}
		else
		{
			D3D_NAME(m_CommandQueue);
		}
	}

	// Assign initial fence values
	m_LastCompletedFenceValue = 0;
	m_NextFenceValue = 1;

	// Create fence
	{
		THROW_IF_FAIL(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)));
		THROW_IF_FAIL(m_Fence->Signal(m_LastCompletedFenceValue));
		D3D_NAME(m_Fence);
	}

	// Create fence event
	{
		m_FenceEventHandle = CreateEventEx(nullptr, L"Queue Fence Event", 0, EVENT_ALL_ACCESS);
		ASSERT(m_FenceEventHandle != INVALID_HANDLE_VALUE, "Failed to create fence event.");
	}
}

D3DQueue::~D3DQueue()
{
	CloseHandle(m_FenceEventHandle);
}

UINT64 D3DQueue::ExecuteCommandLists(UINT count, ID3D12CommandList** ppCommandLists)
{
	m_CommandQueue->ExecuteCommandLists(count, ppCommandLists);
	return Signal();
}

void D3DQueue::InsertWait(UINT64 fenceValue) const
{
	THROW_IF_FAIL(m_CommandQueue->Wait(m_Fence.Get(), fenceValue));
}

void D3DQueue::InsertWaitForQueueFence(const D3DQueue* otherQueue, UINT64 fenceValue) const
{
	THROW_IF_FAIL(m_CommandQueue->Wait(otherQueue->GetFence(), fenceValue));

}

void D3DQueue::InsertWaitForQueue(const D3DQueue* otherQueue) const
{
	THROW_IF_FAIL(m_CommandQueue->Wait(otherQueue->GetFence(), otherQueue->GetNextFenceValue() - 1));
}

void D3DQueue::WaitForFenceCPUBlocking(UINT64 fenceValue)
{
	while (!IsFenceComplete(fenceValue))
	{
		std::lock_guard lockGuard(m_EventMutex);
		if (!IsFenceComplete(fenceValue))
		{
			m_Fence->SetEventOnCompletion(fenceValue, m_FenceEventHandle);
			WaitForSingleObjectEx(m_FenceEventHandle, INFINITE, false);
		}
	}
	m_LastCompletedFenceValue = fenceValue;
}

void D3DQueue::WaitForIdleCPUBlocking()
{
	// Add an additional signal so that when the signaled value is reached we know that all work prior to this call has been completed
	WaitForFenceCPUBlocking(Signal());
}


bool D3DQueue::IsFenceComplete(UINT64 fenceValue)
{
	if (fenceValue > m_LastCompletedFenceValue)
	{
		PollCurrentFenceValue();
	}

	return fenceValue <= m_LastCompletedFenceValue;
}

UINT64 D3DQueue::PollCurrentFenceValue()
{
	const auto fenceValue = m_Fence->GetCompletedValue();
	if (fenceValue > m_LastCompletedFenceValue)
	{
		m_LastCompletedFenceValue = fenceValue;
	}

	return m_LastCompletedFenceValue;
}

UINT64 D3DQueue::Signal()
{
	std::lock_guard lock(m_FenceMutex);
	m_CommandQueue->Signal(m_Fence.Get(), m_NextFenceValue);

	return m_NextFenceValue++;
}
