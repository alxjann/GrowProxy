#pragma once

#include <functional>

struct GLFWwindow;

// The native window the UI is drawn into: GLFW + OpenGL 3 with the Dear ImGui backends.
class Window {
public:
    ~Window();

    bool Create(const char* title, int width, int height);
    void Destroy();

    bool ShouldClose() const;

    // Sets the function that builds the UI of one frame (between ImGui::NewFrame and ImGui::Render).
    void SetDrawCallback(std::function<void()> draw) { m_draw = std::move(draw); }

    // Polls input and draws one frame. While minimized it only waits.
    void Frame();

private:
    void Render();

    std::function<void()> m_draw;
    bool m_rendering = false;
    GLFWwindow* m_window = nullptr;
    bool m_imgui = false;
};
