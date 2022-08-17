#include "Application.hpp"

#include <GLFW/glfw3.h>

#include "Model.hpp"
#include "Texture.hpp"

#include <iostream>

namespace VE
{
    Application::Application()
        : m_Window(WIDTH, HEIGHT, "Vulkan Engine"),
            m_Device(&m_Window)
    {
    }

    void Application::Run()
    {
        const std::vector<Vertex> vertices =
        {
            {{-0.5f, -0.5f, 0.0f},    {0.1686f, 0.1725f, 0.2471f},   {0.0f, 0.0f}},
            {{ 0.5f, -0.5f, 0.0f},    {0.1686f, 0.1725f, 0.2471f},   {1.0f, 0.0f}},
            {{ 0.5f,  0.5f, 0.0f},    {0.1686f, 0.1725f, 0.2471f},   {1.0f, 1.0f}},
            {{-0.5f,  0.5f, 0.0f},    {0.1686f, 0.1725f, 0.2471f},   {0.0f, 1.0f}}
        };

        const std::vector<uint16_t> indices = 
        {
            0, 1, 2, 2, 3, 0
        };

        Model rectangle(&m_Device, vertices, indices);

        Renderer renderer(&m_Window, &m_Device);

        while(!m_Window.ShouldClose())
        {
            m_Window.PollEvents();

            renderer.DrawFrame(rectangle);
        }

        vkDeviceWaitIdle(m_Device.GetVkDevice());
    }
}