#pragma once
#include "Core.h"


class Texture;

using Microsoft::WRL::ComPtr;


class TextureLoader
{
public:
	struct LoadTextureConfig
	{
		DXGI_FORMAT DesiredFormat;
		UINT DesiredChannels;
		UINT BytesPerChannel;
		UINT MipLevels;
		D3D12_RESOURCE_STATES ResourceState;
	};

public:
	TextureLoader();
	~TextureLoader();

	DISALLOW_COPY(TextureLoader);
	DEFAULT_MOVE(TextureLoader);

	std::unique_ptr<Texture> LoadTextureFromFile(const std::string& filename, const LoadTextureConfig* const config);

	void PerformUploads();

private:
	// For executing upload commands
	struct FrameResources
	{
		ComPtr<ID3D12CommandAllocator> CommandAllocator;
		ComPtr<ID3D12GraphicsCommandList> CommandList;
	};
	std::vector<FrameResources> m_FrameResources;

	struct UploadJob
	{
		unsigned char* pData;
		Texture* Destination;

		D3D12_RESOURCE_STATES ResourceState;	// Destination resource state after upload has completed
		UINT ElementStride;						// Number of bytes per element in the resource
	};
	std::queue<UploadJob> m_UploadJobs;

	UINT64 m_LastWorkFence = 0;

};
