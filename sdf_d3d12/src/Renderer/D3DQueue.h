#pragma once

#include "Core.h"

using Microsoft::WRL::ComPtr;


class D3DQueue
{
public:
	D3DQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, const wchar_t* name = nullptr);
	~D3DQueue();

	DISALLOW_COPY(D3DQueue)
	DEFAULT_MOVE(D3DQueue)
	
	// Execute work
	UINT64 ExecuteCommandLists(UINT count, ID3D12CommandList** ppCommandLists);

	// GPU wait
	void InsertWait(UINT64 fenceValue) const;
	void InsertWaitForQueueFence(const D3DQueue* otherQueue, UINT64 fenceValue) const;
	void InsertWaitForQueue(const D3DQueue* otherQueue) const;

	// CPU wait
	void WaitForFenceCPUBlocking(UINT64 fenceValue);
	void WaitForIdleCPUBlocking();

	// Polling
	bool IsFenceComplete(UINT64 fenceValue);
	UINT64 PollCurrentFenceValue();
	inline UINT64 GetLastCompletedFence() const { return m_LastCompletedFenceValue; }
	inline UINT64 GetNextFenceValue() const { return m_NextFenceValue; }

	// Resources
	inline ID3D12CommandQueue* GetCommandQueue() const { return m_CommandQueue.Get(); }
	inline ID3D12Fence* GetFence() const { return m_Fence.Get(); }

private:
	UINT64 Signal();
	
private:
	// D3D12 resources
	ComPtr<ID3D12CommandQueue> m_CommandQueue;
	ComPtr<ID3D12Fence> m_Fence;

	D3D12_COMMAND_LIST_TYPE m_QueueType;

	// Mutexes
	std::mutex m_EventMutex;
	std::mutex m_FenceMutex;

	UINT64 m_NextFenceValue = -1;
	UINT64 m_LastCompletedFenceValue = -1;

	HANDLE m_FenceEventHandle = nullptr;
};
