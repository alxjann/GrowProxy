#include "UI/Window/Window.h"

#include "Core/Log.h"
#include "UI/Theme/Theme.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>

namespace {

    void OnGlfwError(int code, const char* description) {
        Log::Error("[UI] GLFW error %d: %s", code, description);
    }

    // Windows 10/11: dark title bar matching the app background.
    void UseDarkTitleBar(GLFWwindow* window) {
        HWND hwnd = glfwGetWin32Window(window);
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));

        ImVec4 bg = Theme::Background;
        COLORREF caption = RGB((int)(bg.x * 255), (int)(bg.y * 255), (int)(bg.z * 255));
        DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR, Windows 11 */, &caption, sizeof(caption));
    }

}

Window::~Window() {
    Destroy();
}

bool Window::Create(const char* title, int width, int height) {
    glfwSetErrorCallback(OnGlfwError);
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // shown after the first frame, so it never flashes white

    float scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    if (scale <= 0.0f)
        scale = 1.0f;

    m_window = glfwCreateWindow((int)(width * scale), (int)(height * scale), title, nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        return false;
    }

    glfwSetWindowSizeLimits(m_window, (int)(860 * scale), (int)(560 * scale), GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    // Windows blocks glfwPollEvents for as long as the window is being resized; it asks for repaints through
    // this callback instead, so drawing here keeps the UI live during the resize.
    glfwSetWindowUserPointer(m_window, this);
    glfwSetWindowRefreshCallback(m_window, [](GLFWwindow* window) {
        if (Window* self = (Window*)glfwGetWindowUserPointer(window))
            self->Render();
    });
    UseDarkTitleBar(m_window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // the layout is fixed, nothing worth saving
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    Theme::Apply(scale);

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    m_imgui = true;
    return true;
}

void Window::Destroy() {
    if (m_imgui) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_imgui = false;
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
    }
}

bool Window::ShouldClose() const {
    return !m_window || glfwWindowShouldClose(m_window);
}

void Window::Frame() {
    if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED)) {
        glfwWaitEventsTimeout(0.1);
        return;
    }
    glfwPollEvents(); // may call Render itself through the refresh callback
    Render();
}

void Window::Render() {
    if (m_rendering || !m_imgui || !m_draw)
        return;
    m_rendering = true;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    m_draw();
    ImGui::Render();

    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    glViewport(0, 0, width, height);
    ImVec4 bg = Theme::Background;
    glClearColor(bg.x, bg.y, bg.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);

    if (!glfwGetWindowAttrib(m_window, GLFW_VISIBLE))
        glfwShowWindow(m_window);
    m_rendering = false;
}
