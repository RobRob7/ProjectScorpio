#ifndef MIP_GEN_DX12_H
#define MIP_GEN_DX12_H

#include "descriptor_set_dx12.h"
#include "compute_pipeline_dx12.h"

#include <memory>
#include <vector>

class DX12Main;
class ImageDX12;
class ComputeShaderDX12;

class MipGenDX12
{
public:
	explicit MipGenDX12(DX12Main& dx);
	~MipGenDX12();

	void generate(
		ImageDX12& image,
		ID3D12GraphicsCommandList* cmd
	);

private:
	void createDescriptorSets();
	void createPipeline();
private:
	DX12Main* dx_{ nullptr };

	std::unique_ptr<ComputeShaderDX12> shader_;

	std::vector<DescriptorSetDX12> descriptorSets_;
	ComputePipelineDX12 pipeline_;
};

#endif
