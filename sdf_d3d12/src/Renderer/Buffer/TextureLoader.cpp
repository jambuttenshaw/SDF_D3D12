#include "pch.h"
#include "TextureLoader.h"

#include "Texture.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "Renderer/D3DGraphicsContext.h"


TextureLoader::TextureLoader()
	: m_FrameResources(D3DGraphicsContext::GetBackBufferCount())
{
	const auto device = g_D3DGraphicsContext->GetDevice();
	for (auto& resources : m_FrameResources)
	{
		THROW_IF_FAIL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&resources.CommandAllocator)));
		THROW_IF_FAIL(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, resources.CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&resources.CommandList)));

		THROW_IF_FAIL(resources.CommandList->Close());
	}
}

TextureLoader::~TextureLoader()
{
	// Wait until work has completed to not destroy resources that are in use
	g_D3DGraphicsContext->GetDirectCommandQueue()->WaitForFenceCPUBlocking(m_LastWorkFence);
}


std::unique_ptr<Texture> TextureLoader::LoadTextureFromFile(const std::string& filename, const LoadTextureConfig* const config)
{
	int width, height, channels;

	unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, static_cast<int>(config->DesiredChannels));
	ASSERT(data, "Load from file failed");
	ASSERT(channels == config->DesiredChannels, "Desired channels not met!");

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
		config->DesiredFormat,
		width,
		height,
		1,
		1, // Assume file only contains 1 mip level
		1, // sample count
		0, // sample quality
		D3D12_RESOURCE_FLAG_NONE
	);

	// Set resource to be copied into
	std::unique_ptr<Texture> tex = std::make_unique<Texture>(&desc, D3D12_RESOURCE_STATE_COPY_DEST);

	// Create an upload job
	m_UploadJobs.push({});
	auto& job = m_UploadJobs.back();

	job.pData = data;
	job.FirstSubresource = 0;
	job.NumSubresources = 1;
	job.Destination = tex.get();
	job.ResourceState = config->ResourceState;
	job.ElementStride = config->BytesPerChannel * channels;

	return tex;
}


std::unique_ptr<Texture> TextureLoader::LoadTextureCubeFromFile(const std::string& filename, const LoadTextureConfig* const config)
{
	int width, height, channels;

	unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, static_cast<int>(config->DesiredChannels));
	ASSERT(data, "Load from file failed");
	ASSERT(channels == config->DesiredChannels, "Desired channels not met!");
	ASSERT(width == 6 * height, "Invalid cubemap format! It should be laid out as 6 square faces in one row.");

	const UINT faceWidth = width / 6;
	const UINT faceHeight = height;

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
		config->DesiredFormat,
		faceWidth,
		faceHeight,
		6,
		1, // Assume file only contains 1 mip level
		1, // sample count
		0, // sample quality
		D3D12_RESOURCE_FLAG_NONE
	);

	// Set resource to be copied into
	std::unique_ptr<Texture> tex = std::make_unique<Texture>(&desc, D3D12_RESOURCE_STATE_COPY_DEST);

	// Create an upload job
	m_UploadJobs.push({});
	auto& job = m_UploadJobs.back();

	job.pData = data;
	job.FirstSubresource = 0;
	job.NumSubresources = 6;
	job.Destination = tex.get();
	job.ResourceState = config->ResourceState;
	job.ElementStride = config->BytesPerChannel * channels;

	return tex;
}


void TextureLoader::PerformUploads()
{
	if (m_UploadJobs.empty())
		return;

	// Reset command allocator and list
	const auto& resources = m_FrameResources.at(g_D3DGraphicsContext->GetCurrentBackBuffer());
	THROW_IF_FAIL(resources.CommandAllocator->Reset());
	THROW_IF_FAIL(resources.CommandList->Reset(resources.CommandAllocator.Get(), nullptr));

	
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	barriers.reserve(m_UploadJobs.size());

	while (!m_UploadJobs.empty())
	{
		// Job will automatically be deleted when it leaves scope
		const auto job = std::move(m_UploadJobs.front());
		m_UploadJobs.pop();

		const D3D12_RESOURCE_DESC texDesc = job.Destination->GetResource()->GetDesc();
		

		// Create the upload buffer to copy the data into the texture
		ComPtr<ID3D12Resource> textureUploadHeap;

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(job.Destination->GetResource(), job.FirstSubresource, job.NumSubresources);

		const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadHeap)));

		std::vector<D3D12_SUBRESOURCE_DATA> textureData(job.NumSubresources);
		for (UINT i = 0; i < job.NumSubresources; i++)
		{
			textureData[i].pData = job.pData + texDesc.Width * i * job.ElementStride;
			textureData[i].RowPitch = texDesc.Width * job.NumSubresources * job.ElementStride;
			textureData[i].SlicePitch = textureData[i].RowPitch * texDesc.Height;
		}

		// Copy intermediate data to GPU and 
		UpdateSubresources(resources.CommandList.Get(), job.Destination->GetResource(), textureUploadHeap.Get(), 0, job.FirstSubresource, job.NumSubresources, textureData.data());
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(job.Destination->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, job.ResourceState));

		// Delete texture data
		stbi_image_free(job.pData);

		// upload buffer should not be released until work has completed
		g_D3DGraphicsContext->DeferRelease(textureUploadHeap);
	}

	// Submit all resource barriers in one batch
	resources.CommandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

	// Submit work
	{
		THROW_IF_FAIL(resources.CommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { resources.CommandList.Get() };
		m_LastWorkFence = g_D3DGraphicsContext->GetDirectCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}
}
