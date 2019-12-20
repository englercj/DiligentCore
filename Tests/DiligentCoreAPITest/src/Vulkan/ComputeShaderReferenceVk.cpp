/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "Vulkan/TestingEnvironmentVk.h"
#include "Vulkan/TestingSwapChainVk.h"

#include "DeviceContextVk.h"

#include "volk/volk.h"

namespace Diligent
{

namespace Testing
{

static const char* CSSource = R"(
#version 430 core

layout(rgba8, binding = 0) uniform writeonly image2D g_tex2DUAV;

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
 
void main()
{
	ivec2 Dim = imageSize(g_tex2DUAV);
	if (gl_GlobalInvocationID.x >= uint(Dim.x) || gl_GlobalInvocationID.y >= uint(Dim.y))
		return;

	imageStore(g_tex2DUAV, ivec2(gl_GlobalInvocationID.xy), vec4( vec2(gl_GlobalInvocationID.xy % 256u) / 256.0, 0.0, 1.0) );
}
)";

void ComputeShaderReferenceVk(ISwapChain* pSwapChain)
{
    auto* pEnv     = TestingEnvironmentVk::GetInstance();
    auto  vkDevice = pEnv->GetVkDevice();
    auto* pContext = pEnv->GetDeviceContext();

    VkResult res = VK_SUCCESS;
    (void)res;

    auto* pTestingSwapChainVk = ValidatedCast<TestingSwapChainVk>(pSwapChain);

    const auto& SCDesc = pSwapChain->GetDesc();

    auto vkCSModule = pEnv->CreateShaderModule(SHADER_TYPE_COMPUTE, CSSource, static_cast<int>(strlen(CSSource)));
    ASSERT_TRUE(vkCSModule != VK_NULL_HANDLE);

    VkComputePipelineCreateInfo PipelineCI = {};

    PipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    PipelineCI.pNext = nullptr;

    PipelineCI.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    PipelineCI.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    PipelineCI.stage.module = vkCSModule;
    PipelineCI.stage.pName  = "main";

    VkPipelineLayoutCreateInfo PipelineLayoutCI = {};

    VkDescriptorSetLayoutCreateInfo DescriptorSetCI = {};

    VkDescriptorSetLayoutBinding Bindings[1] = {};

    Bindings[0].binding         = 0;
    Bindings[0].descriptorCount = 1;
    Bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    Bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    DescriptorSetCI.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    DescriptorSetCI.bindingCount = 1;
    DescriptorSetCI.pBindings    = Bindings;

    VkDescriptorSetLayout vkSetLayout = VK_NULL_HANDLE;

    res = vkCreateDescriptorSetLayout(vkDevice, &DescriptorSetCI, nullptr, &vkSetLayout);
    ASSERT_GE(res, 0);
    ASSERT_TRUE(vkSetLayout != VK_NULL_HANDLE);

    PipelineLayoutCI.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    PipelineLayoutCI.setLayoutCount = 1;
    PipelineLayoutCI.pSetLayouts    = &vkSetLayout;
    VkPipelineLayout vkLayout       = VK_NULL_HANDLE;
    res                             = vkCreatePipelineLayout(vkDevice, &PipelineLayoutCI, nullptr, &vkLayout);
    ASSERT_GE(res, 0);
    ASSERT_TRUE(vkLayout != VK_NULL_HANDLE);
    PipelineCI.layout = vkLayout;

    VkPipeline vkPipeline = VK_NULL_HANDLE;
    res                   = vkCreateComputePipelines(vkDevice, VK_NULL_HANDLE, 1, &PipelineCI, nullptr, &vkPipeline);
    ASSERT_GE(res, 0);
    ASSERT_TRUE(vkPipeline != VK_NULL_HANDLE);

    VkDescriptorPoolCreateInfo DescriptorPoolCI = {};

    DescriptorPoolCI.sType            = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    DescriptorPoolCI.maxSets          = 1;
    DescriptorPoolCI.poolSizeCount    = 1;
    VkDescriptorPoolSize PoolSizes[1] = {};
    PoolSizes[0].type                 = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    PoolSizes[0].descriptorCount      = 1;
    DescriptorPoolCI.pPoolSizes       = PoolSizes;

    VkDescriptorPool vkDescriptorPool = VK_NULL_HANDLE;

    res = vkCreateDescriptorPool(vkDevice, &DescriptorPoolCI, nullptr, &vkDescriptorPool);
    ASSERT_GE(res, 0);
    ASSERT_TRUE(vkDescriptorPool != VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo SetAllocInfo = {};

    SetAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    SetAllocInfo.descriptorPool     = vkDescriptorPool;
    SetAllocInfo.descriptorSetCount = 1;
    SetAllocInfo.pSetLayouts        = &vkSetLayout;

    VkDescriptorSet vkDescriptorSet = VK_NULL_HANDLE;
    vkAllocateDescriptorSets(vkDevice, &SetAllocInfo, &vkDescriptorSet);
    ASSERT_TRUE(vkDescriptorSet != VK_NULL_HANDLE);

    VkWriteDescriptorSet DescriptorWrite = {};

    DescriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    DescriptorWrite.dstSet          = vkDescriptorSet;
    DescriptorWrite.dstBinding      = 0;
    DescriptorWrite.dstArrayElement = 0;
    DescriptorWrite.descriptorCount = 1;
    DescriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    VkDescriptorImageInfo ImageInfo = {};
    ImageInfo.imageView             = pTestingSwapChainVk->GetVkRenderTargetImageView();
    ImageInfo.imageLayout           = VK_IMAGE_LAYOUT_GENERAL;
    DescriptorWrite.pImageInfo      = &ImageInfo;

    vkUpdateDescriptorSets(vkDevice, 1, &DescriptorWrite, 0, nullptr);

    VkCommandBuffer vkCmdBuffer = pEnv->AllocateCommandBuffer();

    pTestingSwapChainVk->TransitionRenderTarget(vkCmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vkCmdBindDescriptorSets(vkCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkLayout, 0, 1, &vkDescriptorSet, 0, nullptr);
    vkCmdBindPipeline(vkCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipeline);
    vkCmdDispatch(vkCmdBuffer, (SCDesc.Width + 15) / 16, (SCDesc.Height + 15) / 16, 1);

    res = vkEndCommandBuffer(vkCmdBuffer);
    VERIFY(res >= 0, "Failed to end command buffer");

    RefCntAutoPtr<IDeviceContextVk> pContextVk{pContext, IID_DeviceContextVk};

    auto* pQeueVk = pContextVk->LockCommandQueue();
    auto  vkQueue = pQeueVk->GetVkQueue();

    VkSubmitInfo SubmitInfo       = {};
    SubmitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.pCommandBuffers    = &vkCmdBuffer;
    SubmitInfo.commandBufferCount = 1;
    vkQueueSubmit(vkQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vkQueue);

    pContextVk->UnlockCommandQueue();

    vkDestroyDescriptorPool(vkDevice, vkDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vkDevice, vkSetLayout, nullptr);
    vkDestroyPipelineLayout(vkDevice, vkLayout, nullptr);
    vkDestroyPipeline(vkDevice, vkPipeline, nullptr);
    vkDestroyShaderModule(vkDevice, vkCSModule, nullptr);
}

} // namespace Testing

} // namespace Diligent