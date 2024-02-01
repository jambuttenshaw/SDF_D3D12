#include "pch.h"
#include "SDFFactorySync.h"

#include "Renderer/D3DGraphicsContext.h"


#include "SDF/SDFObject.h"
#include "SDF/SDFEditList.h"

#include "pix3.h"


void SDFFactorySync::BakeSDFSynchronous(SDFObject* object, const SDFEditList& editList, bool cpuWaitUntilComplete)
{
	LOG_TRACE("-----SDF Factory Synchronous Bake Begin--------");
	PIXBeginEvent(PIX_COLOR_INDEX(12), L"SDF Bake Synchronous");

	const auto directQueue = g_D3DGraphicsContext->GetDirectCommandQueue();
	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();

	// Make sure the last bake to have occurred has completed
	PIXBeginEvent(PIX_COLOR_INDEX(14), L"Wait for previous bake");
	computeQueue->WaitForFenceCPUBlocking(m_PreviousBakeFence);
	PIXEndEvent();
	// Make the compute queue wait until the render queue has finished its work
	computeQueue->InsertWaitForQueue(directQueue);

	object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTING);

	PerformSDFBake_CPUBlocking(object, editList);

	// The rendering queue should wait until these operations have completed
	directQueue->InsertWaitForQueue(computeQueue);

	// The CPU can optionally wait until the operations have completed too
	if (cpuWaitUntilComplete)
	{
		PIXBeginEvent(PIX_COLOR_INDEX(13), L"Wait for bake completion");
		computeQueue->WaitForFenceCPUBlocking(m_PreviousBakeFence);
		PIXEndEvent();
	}

	object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTED);

	PIXEndEvent();
	LOG_TRACE("-----SDF Factory Synchronous Bake Complete-----");
}

