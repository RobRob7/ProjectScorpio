//#include "acceleration_structure_vk.h"
//
//#include "constants.h"
//
//#include "vulkan_main.h"
//
//#include <stdexcept>
//#include <cstdint>
//
////--- PUBLIC ---//
//AccelerationStructureVk::AccelerationStructureVk(VulkanMain& vk)
//	: vk_(&vk),
//	instanceBuffer_(vk),
//	scratchBuffer_(vk),
//	buffer_(vk)
//{
//} // end of constructor
//
//AccelerationStructureVk::~AccelerationStructureVk() = default;
//
//void AccelerationStructureVk::destroy()
//{
//	as_.reset();
//	instanceBuffer_.destroy();
//	scratchBuffer_.destroy();
//	buffer_.destroy();
//	deviceAddress_ = 0;
//} // end of destroy()
//
//void AccelerationStructureVk::buildBLASOnCmd(
//	vk::CommandBuffer cmd,
//	vk::Buffer vertexBuffer,
//	vk::DeviceAddress vertexAddress,
//	uint32_t vertexCount,
//	vk::DeviceSize vertexStride,
//	vk::Buffer indexBuffer,
//	vk::DeviceAddress indexAddress,
//	uint32_t indexCount,
//	vk::IndexType indexType
//)
//{
//	if (!vertexBuffer || !indexBuffer)
//	{
//		throw std::runtime_error("AccelerationStructureVk::buildBLAS - invalid vertex/index buffer!");
//	}
//	if (vertexCount == 0 || indexCount == 0 || (indexCount % 3) != 0)
//	{
//		throw std::runtime_error("AccelerationStructureVk::buildBLAS - invalid counts!");
//	}
//	if (vertexAddress == 0 || indexAddress == 0)
//	{
//		throw std::runtime_error("AccelerationStructureVk::buildBLAS - invalid buffer device address!");
//	}
//
//	vk::Device device = vk_->getDevice();
//
//	// triangle geometry
//	vk::AccelerationStructureGeometryTrianglesDataKHR triangles{};
//	triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
//	triangles.vertexData.deviceAddress = vertexAddress;
//	triangles.vertexStride = vertexStride;
//	triangles.maxVertex = vertexCount - 1;
//	triangles.indexType = indexType;
//	triangles.indexData.deviceAddress = indexAddress;
//
//	vk::AccelerationStructureGeometryKHR geometry{};
//	geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
//	geometry.flags = {};
//	geometry.geometry.triangles = triangles;
//
//	const uint32_t primitiveCount = indexCount / 3;
//
//	// build info for size query
//	vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
//	buildInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
//	buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
//	buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
//	buildInfo.geometryCount = 1;
//	buildInfo.pGeometries = &geometry;
//
//	vk::AccelerationStructureBuildSizesInfoKHR sizeInfo =
//		device.getAccelerationStructureBuildSizesKHR(
//			vk::AccelerationStructureBuildTypeKHR::eDevice,
//			buildInfo,
//			{ primitiveCount }
//		);
//
//	// create AS storage buffer
//	buffer_.create(
//		sizeInfo.accelerationStructureSize,
//		vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
//		vk::BufferUsageFlagBits::eShaderDeviceAddress,
//		vk::MemoryPropertyFlagBits::eDeviceLocal,
//		true
//	);
//
//	// create AS handle
//	vk::AccelerationStructureCreateInfoKHR asInfo{};
//	asInfo.buffer = buffer_.getBuffer();
//	asInfo.size = sizeInfo.accelerationStructureSize;
//	asInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
//
//	{
//		vk::ResultValue rv = device.createAccelerationStructureKHRUnique(asInfo);
//		if (rv.result != vk::Result::eSuccess)
//		{
//			throw std::runtime_error("AccelerationStructureVk::buildBLAS - createAccelerationStructureKHRUnique failed: " +
//				vk::to_string(rv.result));
//		}
//		as_ = std::move(rv.value);
//	}
//
//	// scratch buffer
//	scratchBuffer_.create(
//		sizeInfo.buildScratchSize,
//		vk::BufferUsageFlagBits::eStorageBuffer |
//		vk::BufferUsageFlagBits::eShaderDeviceAddress,
//		vk::MemoryPropertyFlagBits::eDeviceLocal,
//		true
//	);
//
//	buildInfo.dstAccelerationStructure = as_.get();
//	buildInfo.scratchData.deviceAddress = scratchBuffer_.getDeviceAddress();
//
//	vk::AccelerationStructureBuildRangeInfoKHR rangeInfo{};
//	rangeInfo.primitiveCount = primitiveCount;
//	rangeInfo.primitiveOffset = 0;
//	rangeInfo.firstVertex = 0;
//	rangeInfo.transformOffset = 0;
//
//	const vk::AccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
//
//	cmd.buildAccelerationStructuresKHR(1, &buildInfo, &pRangeInfo);
//
//	vk::MemoryBarrier barrier{};
//	barrier.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR;
//	barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR |
//		vk::AccessFlagBits::eShaderRead;
//
//	cmd.pipelineBarrier(
//		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
//		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR |
//		vk::PipelineStageFlagBits::eRayTracingShaderKHR,
//		{},
//		1, &barrier,
//		0, nullptr,
//		0, nullptr
//	);
//
//	vk::AccelerationStructureDeviceAddressInfoKHR addrInfo{};
//	addrInfo.accelerationStructure = as_.get();
//	deviceAddress_ = device.getAccelerationStructureAddressKHR(addrInfo);
//} // end of buildBLAS()
//
//void AccelerationStructureVk::buildTLASOnCmd(
//	vk::CommandBuffer cmd,
//	const std::vector<vk::AccelerationStructureInstanceKHR>& instances
//)
//{
//	if (instances.empty())
//	{
//		throw std::runtime_error("AccelerationStructureVk::buildTLASOnCmd - instances cannot be empty!");
//	}
//
//	vk::Device device = vk_->getDevice();
//
//	vk::DeviceSize instanceBufferSize =
//		sizeof(vk::AccelerationStructureInstanceKHR) * instances.size();
//
//	instanceBuffer_.create(
//		instanceBufferSize,
//		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
//		vk::BufferUsageFlagBits::eShaderDeviceAddress,
//		vk::MemoryPropertyFlagBits::eHostVisible |
//		vk::MemoryPropertyFlagBits::eHostCoherent,
//		true
//	);
//	instanceBuffer_.upload(instances.data(), instanceBufferSize);
//
//	vk::AccelerationStructureGeometryInstancesDataKHR instanceData{};
//	instanceData.arrayOfPointers = vk::False;
//	instanceData.data.deviceAddress = instanceBuffer_.getDeviceAddress();
//
//	vk::AccelerationStructureGeometryKHR geometry{};
//	geometry.geometryType = vk::GeometryTypeKHR::eInstances;
//	geometry.geometry.instances = instanceData;
//
//	const uint32_t primitiveCount = static_cast<uint32_t>(instances.size());
//
//	vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
//	buildInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
//	buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
//	buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
//	buildInfo.geometryCount = 1;
//	buildInfo.pGeometries = &geometry;
//
//	vk::AccelerationStructureBuildSizesInfoKHR sizeInfo =
//		device.getAccelerationStructureBuildSizesKHR(
//			vk::AccelerationStructureBuildTypeKHR::eDevice,
//			buildInfo,
//			{ primitiveCount }
//		);
//
//	buffer_.create(
//		sizeInfo.accelerationStructureSize,
//		vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
//		vk::BufferUsageFlagBits::eShaderDeviceAddress,
//		vk::MemoryPropertyFlagBits::eDeviceLocal,
//		true
//	);
//
//	vk::AccelerationStructureCreateInfoKHR asInfo{};
//	asInfo.buffer = buffer_.getBuffer();
//	asInfo.size = sizeInfo.accelerationStructureSize;
//	asInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
//
//	{
//		auto rv = device.createAccelerationStructureKHRUnique(asInfo);
//		if (rv.result != vk::Result::eSuccess)
//		{
//			throw std::runtime_error(
//				"AccelerationStructureVk::buildTLASOnCmd - createAccelerationStructureKHRUnique failed: " +
//				vk::to_string(rv.result)
//			);
//		}
//		as_ = std::move(rv.value);
//	}
//
//	scratchBuffer_.create(
//		sizeInfo.buildScratchSize,
//		vk::BufferUsageFlagBits::eStorageBuffer |
//		vk::BufferUsageFlagBits::eShaderDeviceAddress,
//		vk::MemoryPropertyFlagBits::eDeviceLocal,
//		true
//	);
//
//	buildInfo.dstAccelerationStructure = as_.get();
//	buildInfo.scratchData.deviceAddress = scratchBuffer_.getDeviceAddress();
//
//	vk::AccelerationStructureBuildRangeInfoKHR rangeInfo{};
//	rangeInfo.primitiveCount = primitiveCount;
//	rangeInfo.primitiveOffset = 0;
//	rangeInfo.firstVertex = 0;
//	rangeInfo.transformOffset = 0;
//
//	const vk::AccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
//
//	cmd.buildAccelerationStructuresKHR(1, &buildInfo, &pRangeInfo);
//
//	vk::MemoryBarrier barrier{};
//	barrier.srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR;
//	barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR |
//		vk::AccessFlagBits::eShaderRead;
//
//	cmd.pipelineBarrier(
//		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
//		vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR |
//		vk::PipelineStageFlagBits::eRayTracingShaderKHR,
//		{},
//		1, &barrier,
//		0, nullptr,
//		0, nullptr
//	);
//
//	vk::AccelerationStructureDeviceAddressInfoKHR addrInfo{};
//	addrInfo.accelerationStructure = as_.get();
//	deviceAddress_ = device.getAccelerationStructureAddressKHR(addrInfo);
//} // end of buildTLASOnCmd()
