#include "pch.h"
#include "SDFFactorySync.h"

#include "Renderer/D3DGraphicsContext.h"


#include "SDF/SDFObject.h"
#include "SDF/SDFEditList.h"

#include "pix3.h"


void SDFFactorySync::BakeSDFSynchronous(SDFObject* object, const SDFEditList& editList, bool cpuWaitUntilComplete)
{
	LOG_TRACE("-----SDF Factory Synchronous Bake Begin--------");

	const auto directQueue = g_D3DGraphicsContext->GetDirectCommandQueue();
	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();

	// Make sure the last bake to have occurred has completed
	computeQueue->WaitForFenceCPUBlocking(m_PreviousBakeFence);
	// Make the compute queue wait until the render queue has finished its work
	computeQueue->InsertWaitForQueue(directQueue);

	object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTING);

	PerformSDFBake_CPUBlocking(object, editList);

	// The rendering queue should wait until these operations have completed
	directQueue->InsertWaitForQueue(computeQueue);

	// The CPU can optionally wait until the operations have completed too
	if (cpuWaitUntilComplete)
	{
		computeQueue->WaitForFenceCPUBlocking(m_PreviousBakeFence);
	}

	object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTED);
	
	LOG_TRACE("-----SDF Factory Synchronous Bake Complete-----");
}

