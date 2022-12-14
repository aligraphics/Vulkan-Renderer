#include "Renderer.hpp"

#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"

#include "Utilities.hpp"

#include <stdexcept>
#include <cassert>
#include <chrono>
#include <iostream>
#include <unordered_map>

namespace VE
{
	Renderer::Renderer(Window* window, Device* device)
		:	m_Window(window), m_Device(device), m_Swapchain(m_Device, m_Window),
			m_PipelineLayout(VK_NULL_HANDLE), m_Pipeline(nullptr),
			m_CurrentImageIndex{}
	{
		CreateCommandBuffers();
		CreatePipelineLayout();
		CreatePipeline();
	}

	Renderer::~Renderer()
	{
		if (m_PipelineLayout)
		{
			vkDestroyPipelineLayout(m_Device->GetVkDevice(), m_PipelineLayout, nullptr);
		}
	}

	void Renderer::CreateCommandBuffers()
	{
		m_CommandBuffers.resize(Swapchain::MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_Device->GetCommandPool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

		VK_CHECK(vkAllocateCommandBuffers(m_Device->GetVkDevice(), &allocInfo, m_CommandBuffers.data()))
	}

	void Renderer::CreatePipelineLayout()
	{
		VkPipelineLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = static_cast<uint32_t>(m_Device->GetDescriptorSetLayouts().size());
		layoutCreateInfo.pSetLayouts = m_Device->GetDescriptorSetLayouts().data();

		VK_CHECK(vkCreatePipelineLayout(m_Device->GetVkDevice(), &layoutCreateInfo, nullptr, &m_PipelineLayout))
	}

	void Renderer::CreatePipeline()
	{
		PipelineConfigInfo pipelineConfig{};
		Pipeline::DefaultPipelineConfig(pipelineConfig);

		VkVertexInputBindingDescription bindingDesc{};
		bindingDesc.binding = 0;
		bindingDesc.stride = sizeof(Vertex);
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		pipelineConfig.bindingDescriptions.push_back(bindingDesc);

		VkVertexInputAttributeDescription posDesc{};
		posDesc.binding = 0;
		posDesc.location = 0;
		posDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
		posDesc.offset = offsetof(Vertex, position);
		pipelineConfig.attributeDescriptions.push_back(posDesc);

		VkVertexInputAttributeDescription colorDesc{};
		colorDesc.binding = 0;
		colorDesc.location = 1;
		colorDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
		colorDesc.offset = offsetof(Vertex, color);
		pipelineConfig.attributeDescriptions.push_back(colorDesc);

		VkVertexInputAttributeDescription texCoordDesc{};
		texCoordDesc.binding = 0;
		texCoordDesc.location = 2;
		texCoordDesc.format = VK_FORMAT_R32G32_SFLOAT;
		texCoordDesc.offset = offsetof(Vertex, texCoords);
		pipelineConfig.attributeDescriptions.push_back(texCoordDesc);

		pipelineConfig.renderPass = m_Swapchain.GetRenderPass();
		pipelineConfig.pipelineLayout = m_PipelineLayout;

		m_Pipeline = std::make_unique<Pipeline>(m_Device, pipelineConfig);
	}

	void Renderer::DrawFrame(Model& model)
	{
		VkCommandBuffer currCommandBuffer = GetCurrentCommandBuffer();
		BeginFrame(currCommandBuffer);

		// Draw Here
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(m_Swapchain.GetExtent().width);
		viewport.height = static_cast<float>(m_Swapchain.GetExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_Swapchain.GetExtent();

		vkCmdBindPipeline(currCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetGraphicsPipeline());
		vkCmdSetViewport(currCommandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(currCommandBuffer, 0, 1, &scissor);

		model.UpdateDescriptors(currCommandBuffer, m_PipelineLayout, m_Swapchain.GetCurrentFrame()); // Optimize to only update if resource change
		model.Bind(currCommandBuffer);
		model.Draw(currCommandBuffer);

		EndFrame(currCommandBuffer);
	}

	void Renderer::BeginFrame(VkCommandBuffer commandBuffer)
	{
		VkResult result = m_Swapchain.AcquireNextImage(&m_CurrentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_Swapchain.RecreateSwapchain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("Error: Failed to acquire swap chain image!");
		}

		vkResetFences(m_Device->GetVkDevice(), 1, &m_Swapchain.GetInFlightFences()[m_Swapchain.GetCurrentFrame()]);
		vkResetCommandBuffer(commandBuffer, 0);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;					// Optional
		beginInfo.pInheritanceInfo = nullptr;	// Optional

		VK_CHECK(vkBeginCommandBuffer(m_CommandBuffers[m_Swapchain.GetCurrentFrame()], &beginInfo))

		VkRenderPassBeginInfo renderPassInfo {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_Swapchain.GetRenderPass();
		renderPassInfo.framebuffer = m_Swapchain.GetFramebuffers()[m_CurrentImageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_Swapchain.GetExtent();

		VkClearValue clearValues[2];
		clearValues[0].color = { {0.1137f, 0.1137f, 0.1725f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void Renderer::EndFrame(VkCommandBuffer commandBuffer)
	{
		vkCmdEndRenderPass(commandBuffer);
		VK_CHECK(vkEndCommandBuffer(commandBuffer))

		VkSubmitInfo submitInfo {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { m_Swapchain.GetImageAvailableSemaphores()[m_Swapchain.GetCurrentFrame()] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VkSemaphore signalSemaphores[] = { m_Swapchain.GetRenderFinishedSemaphores()[m_Swapchain.GetCurrentFrame()] };

		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		VK_CHECK(vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, m_Swapchain.GetInFlightFences()[m_Swapchain.GetCurrentFrame()]))

		VkPresentInfoKHR presentInfo {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		VkSwapchainKHR swapchains[] = { m_Swapchain.GetSwapchain() };

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &m_CurrentImageIndex;

		VkResult result = vkQueuePresentKHR(m_Device->GetPresentQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			m_Swapchain.RecreateSwapchain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("Error: Failed to present swapchain image!");
		}

		uint32_t nextFrame = (m_Swapchain.GetCurrentFrame() + 1) % Swapchain::MAX_FRAMES_IN_FLIGHT;
		m_Swapchain.SetCurrentFrame(nextFrame);
	}
}
