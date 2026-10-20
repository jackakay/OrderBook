// src/main.cpp
#include <iostream>

// 1. MUST include GLAD before GLFW
#include "glad/glad.h"
#include "GLFW/glfw3.h"

// 2. Dear ImGui headers
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Global Window pointer so helper functions can access it
GLFWwindow* window = nullptr;

// UI State Buffers
char buffer[256] = "Type here...";

// Function prototypes
bool init();
void shutdown();

int main() {
    if (!init()) {
        std::cerr << "Initialization failed!" << std::endl;
        return -1;
    }

    // Main application loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Example GUI Window
        if (ImGui::Begin("Market GUI Example")) {
            ImGui::Text("Hello, world from local setup!");
            ImGui::InputText("Text Input", buffer, sizeof(buffer));
            
            if (ImGui::Button("Click Me")) {
                std::cout << "Button clicked! Input text was: " << buffer << std::endl;
            }
            ImGui::End();
        }

        // Render ImGui
        ImGui::Render();

        // Clear screen background (Dark blue-grey)
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw ImGui drop data onto screen
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup resources
    shutdown();
    return 0;
}

bool init() {
    // Initialize GLFW
    if (!glfwInit())
        return false;

    // Configure OpenGL Version (3.3 Core Profile)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    window = glfwCreateWindow(1280, 720, "Market Application", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync

    // Initialize GLAD (Loads OpenGL function pointers)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD loader!" << std::endl;
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

void shutdown() {
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup GLFW window and context
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}