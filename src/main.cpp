#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>
#include <iostream>
#include "shader.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "structs.h"
#include "camera.h"
#include "mesh.h"
#include "aabb.h"
#include "bvh.h"
#include "textures.h"
#include <random>

// === ImGui ===
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

int WIDTH = 1920, HEIGHT = 1080;

glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  0.0f);
glm::vec3 WorldUp    = glm::vec3(0.0f, 1.0f,  0.0f);

float lastFrame = 0.0f;
float deltaTime = 0.0f;
float lastX = 400, lastY = 300;
float yaw = -90.0f, pitch = 0.0f;
bool firstMouse = true;
unsigned int frames = 1;
float defocus_angle = 0.0f;
float focus_dist = 3.4f;
bool saveRequested = false;
glm::vec4 background_color = glm::vec4(0.02f, 0.02f, 0.03f, 1.0f);
float exposure = 1.0f;
float envIntensity = 1.5f;
int useHDRI = 0;

// === GUI state ===
bool guiMode = false;          // Tab toggles this; cursor visible when true
int selectedMaterial = -1;     // -1 = nothing selected (no highlight)

Camera cam = Camera(cameraPos, WorldUp, yaw, pitch);

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void process_input(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void saveFramebufferToPNG(GLuint texture, int width, int height, const std::string& filename);

struct CameraUBO {
    glm::vec4 camPosition;
    glm::vec4 cameraRight;
    glm::vec4 cameraUp;
    glm::vec4 cameraForward;
    glm::ivec4 screenData;
    glm::vec4 cameraData;
    glm::vec4 backGround_color;
};

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Path Tracer", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // === Initialize ImGui ===
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    //======Setting up the framebuffers for playing ping-pong======
    unsigned int tracer;
    unsigned int display;
    glGenFramebuffers(1, &tracer);
    glGenFramebuffers(1, &display);

    unsigned int texture_tracer;
    unsigned int texture_display;
    glGenTextures(1, &texture_tracer);
    glGenTextures(1, &texture_display);
    glBindTexture(GL_TEXTURE_2D, texture_tracer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, texture_display);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, tracer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_tracer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        std::cout << "ERROR::FRAMEBUFFER_SETUP::TRACER";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, display);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_display, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        std::cout << "ERROR::FRAMEBUFFER_SETUP::DISPLAY";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned int ping_pong_buffer[] = {tracer, display};
    unsigned int ping_pong_texture[] = {texture_tracer, texture_display};
    unsigned int curr_write_buffer = 0;

    //======MAKING BUFFER FOR THE SCREEN QUAD======
    float screen_quad[] = {
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f
    };
    unsigned int screen_indices[] = { 0, 3, 2, 0, 2, 1 };

    unsigned int Screen_Quad_VAO;
    glGenVertexArrays(1, &Screen_Quad_VAO);
    glBindVertexArray(Screen_Quad_VAO);
    unsigned int Screen_Quad_VBO;
    glGenBuffers(1, &Screen_Quad_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, Screen_Quad_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screen_quad), screen_quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    unsigned int Screen_Quad_EBO;
    glGenBuffers(1, &Screen_Quad_EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Screen_Quad_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(screen_indices), screen_indices, GL_STATIC_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    //Making the Camera UBO
    static_assert(sizeof(CameraUBO) == 112, "");
    CameraUBO camData{};
    unsigned int cameraUBO;
    glGenBuffers(1, &cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBO), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //===========SETTING UP THE SCENE HDRI============
    unsigned int hdriTexture;
    {
        int width, height, channels;
        float *data = stbi_loadf("sunset.hdr", &width, &height, &channels, 3);
        if(!data){
            std::cout << "Failed to load HDR: " << stbi_failure_reason() << std::endl;
        }else{
            std::cout << "Loaded HDRI file successfully" <<std::endl;
        }
        glGenTextures(1, &hdriTexture);
        glBindTexture(GL_TEXTURE_2D, hdriTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);
    }

    //Making the World Objects here
    std::vector<Material> materials;
    std::vector<GPUSphere> spheres;
    std::vector<GPUQuad> quads;
    std::vector<GPUVertex> vertices;
    std::vector<GPUIndex> indices;
    std::vector<GPUMediumSphere> mediumSpheres;
    TextureArray textures(1024, 1024, 16);

    //BVH Objects
    std::vector<PrimitiveRef> refs;
    std::vector<GPUBVHNode> gpu_bvh;
    std::vector<GPUPrimitiveRef> gpu_refs;

    //==================== PRE-LOAD ALL MESHES ONCE ====================
    Mesh bunnyMesh;
    bunnyMesh.loadOBJ("Bunny.obj");
    Mesh dragonMesh;
    dragonMesh.loadOBJ("Dragon.obj");
    Mesh breakfastMesh;
    breakfastMesh.loadOBJ("BreakFast.obj");

    //Passing the world objects using an SSBO
    unsigned int sphereBuffer, materialBuffer, quadBuffer, vertexBuffer, indexBuffer, bvhBuffer, primRefsBuffer, mediumSphereBuffer;
    glGenBuffers(1, &sphereBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(GPUSphere), spheres.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereBuffer);
    glGenBuffers(1, &materialBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(Material), materials.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, materialBuffer);
    glGenBuffers(1, &quadBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, quadBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, quads.size() * sizeof(GPUQuad), quads.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, quadBuffer);
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(GPUVertex), vertices.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, vertexBuffer);
    glGenBuffers(1, &indexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(GPUIndex), indices.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, indexBuffer);
    glGenBuffers(1, &bvhBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_bvh.size() * sizeof(GPUBVHNode), gpu_bvh.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, bvhBuffer);
    glGenBuffers(1, &primRefsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, primRefsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_refs.size() * sizeof(GPUPrimitiveRef), gpu_refs.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, primRefsBuffer);
    glGenBuffers(1, &mediumSphereBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mediumSphereBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, mediumSpheres.size() * sizeof(GPUMediumSphere), mediumSpheres.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, mediumSphereBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // ===============================================SCENE-BUILDING LAMBDAS========================================================

    auto rebuildBVH = [&]() {
        refs.clear();
        aabb ab;
        for (int i = 0; i < (int)spheres.size(); i++) {
            refs.push_back({0, i, ab.sphere_aabb(spheres[i]), ab.sphere_centroid(spheres[i])});
        }
        for (int i = 0; i < (int)quads.size(); i++) {
            refs.push_back({1, i, ab.quad_aabb(quads[i]), ab.quad_centroid(quads[i])});
        }
        for (int i = 0; i < (int)indices.size(); i++) {
            refs.push_back({2, i, ab.triangle_aabb(indices[i], vertices), ab.triangle_centroid(indices[i], vertices)});
        }
        for (int i = 0; i < (int)mediumSpheres.size(); i++) {
            refs.push_back({3, i, ab.medium_sphere_aabb(mediumSpheres[i]), ab.medium_sphere_centroid(mediumSpheres[i])});
        }

        bvh_node root(refs, 0, (int)refs.size());
        std::cout << "BVH built: " << root.count_nodes() << " nodes, depth " << root.max_depth() << "\n";
        gpu_bvh = root.flatten();

        gpu_refs.clear();
        gpu_refs.reserve(refs.size());
        for (const auto& r : refs) {
            GPUPrimitiveRef gr;
            gr.data = glm::ivec4(r.primitive_type, r.index, 0, 0);
            gpu_refs.push_back(gr);
        }
    };

    auto uploadAllBuffers = [&]() {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(GPUSphere), spheres.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(Material), materials.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, quadBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, quads.size() * sizeof(GPUQuad), quads.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(GPUVertex), vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(GPUIndex), indices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_bvh.size() * sizeof(GPUBVHNode), gpu_bvh.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, primRefsBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_refs.size() * sizeof(GPUPrimitiveRef), gpu_refs.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mediumSphereBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, mediumSpheres.size() * sizeof(GPUMediumSphere), mediumSpheres.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    };

    // Re-uploads just the material buffer (used by the material editor for fast updates).
    auto uploadMaterials = [&]() {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, materials.size() * sizeof(Material), materials.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    };

    auto clearScene = [&]() {
        materials.clear();
        spheres.clear();
        quads.clear();
        vertices.clear();
        indices.clear();
        mediumSpheres.clear();
        selectedMaterial = -1;  // Reset selection when scene changes
    };

    auto makeQuad = [](glm::vec3 Q, glm::vec3 u, glm::vec3 v, int mat) {
        GPUQuad q;
        q.Q = glm::vec4(Q, float(mat));
        q.u = glm::vec4(u, 0.0f);
        q.v = glm::vec4(v, 0.0f);
        return q;
    };

    auto makeSphere = [](glm::vec3 c, float r, int m) {
        GPUSphere s;
        s.center = glm::vec4(c, r);
        s.extra = glm::vec4(float(m), 0.0f, 0.0f, 0.0f);
        return s;
    };

    // SCENE 1: Scattered emissive spheres
    auto scattered_spheres = [&]() {
        clearScene();
        materials.push_back({glm::vec4(0.85f, 0.85f, 0.85f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.2f, 0.2f, 0.25f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.5f, 0.15f, 0.6f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.15f, 0.5f, 0.5f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.6f, 0.2f, 0.2f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.2f, 0.4f, 0.6f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.4f, 0.35f, 0.2f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.3f, 0.3f, 0.3f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.9f, 0.9f, 0.92f, 1.0f), glm::vec4(0.05f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.8f, 0.7f, 0.4f, 1.0f), glm::vec4(0.1f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(1.0f, 1.0f, 1.0f, 2.0f), glm::vec4(0.0f, 1.5f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)});
        materials.push_back({glm::vec4(1.0f, 1.0f, 1.0f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.3f, 1.0f, 0.3f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(1.0f, 0.3f, 1.0f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.3f, 0.5f, 1.0f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(1.0f, 0.9f, 0.3f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.3f, 1.0f, 1.0f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(1.0f, 0.5f, 0.2f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.08f, 0.08f, 0.1f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});

        quads.push_back(makeQuad(
            glm::vec3(-30.0f, -2.0f, -30.0f),
            glm::vec3(60.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 60.0f),
            18
        ));

        int lambertianMats[] = {0, 1, 2, 3, 4, 5, 6, 7};
        int specularMats[]   = {8, 9, 10};
        int emissiveMats[]   = {11, 12, 13, 14, 15, 16, 17};

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> distX(-4.0f, 4.0f);
        std::uniform_real_distribution<float> distZ(-9.0f, -3.0f);
        std::uniform_real_distribution<float> distRadius(0.08f, 0.5f);
        std::uniform_real_distribution<float> distMatPick(0.0f, 1.0f);

        int NUM_SPHERES = 150;
        for (int i = 0; i < NUM_SPHERES; i++) {
            float r = distRadius(rng);
            float roll = distMatPick(rng);
            int mat;
            if (roll < 0.6f)      mat = lambertianMats[rng() % 8];
            else if (roll < 0.8f) mat = specularMats[rng() % 3];
            else                  mat = emissiveMats[rng() % 7];
            glm::vec3 center(distX(rng), -2.0f + r, distZ(rng));
            spheres.push_back(makeSphere(center, r, mat));
        }

        useHDRI = 0;
        background_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        cam.Position = glm::vec3(0.0f, 1.0f, 0.0f);

        rebuildBVH();
        uploadAllBuffers();
        cam.moved = true;
    };

    // SCENE 2: Glass bunny
    auto glass_bunny = [&]() {
        clearScene();
        materials.push_back({glm::vec4(0.73f, 0.73f, 0.73f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.65f, 0.05f, 0.05f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.12f, 0.45f, 0.15f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(1.0f, 0.95f, 0.85f, 3.0f), glm::vec4(0.0f, 0.0f, 15.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(1.0f, 1.0f, 1.0f, 2.0f), glm::vec4(0.0f, 1.5f, 0.0f, 0.0f), glm::vec4(0.15f, 1.5f, 1.8f, 0.0f)});

        quads.push_back(makeQuad({-3.0f, -3.0f, -3.0f}, {6.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -6.0f}, 0));
        quads.push_back(makeQuad({-3.0f,  3.0f, -3.0f}, {6.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -6.0f}, 0));
        quads.push_back(makeQuad({-3.0f, -3.0f, -9.0f}, {6.0f, 0.0f, 0.0f}, {0.0f, 6.0f,  0.0f}, 0));
        quads.push_back(makeQuad({-3.0f, -3.0f, -3.0f}, {0.0f, 6.0f, 0.0f}, {0.0f, 0.0f, -6.0f}, 1));
        quads.push_back(makeQuad({ 3.0f, -3.0f, -3.0f}, {0.0f, 6.0f, 0.0f}, {0.0f, 0.0f, -6.0f}, 2));
        quads.push_back(makeQuad({-1.2f, 2.99f, -5.0f}, {2.4f, 0.0f, 0.0f}, {0.0f, 0.0f, -2.4f}, 3));

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(0.0f, -3.0f, -6.0f));
        transform = glm::scale(transform, glm::vec3(15.0f));
        bunnyMesh.appendToScene(vertices, indices, 4, transform);

        useHDRI = 0;
        background_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        cam.Position = glm::vec3(0.0f, -0.3f, 0.0f);

        rebuildBVH();
        uploadAllBuffers();
        cam.moved = true;
    };

    // SCENE 3: Glass dragon
    auto glass_dragon = [&]() {
        clearScene();
        materials.push_back({glm::vec4(0.78f, 0.78f, 0.78f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.65f, 0.05f, 0.05f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.12f, 0.45f, 0.15f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(1.0f, 0.95f, 0.85f, 3.0f), glm::vec4(0.0f, 0.0f, 15.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(1.0f, 1.0f, 1.0f, 2.0f), glm::vec4(0.0f, 1.5f, 0.0f, 0.0f), glm::vec4(1.5f, 0.15f, 1.2f, 0.0f)});

        quads.push_back(makeQuad({-3.0f, -3.0f, -3.0f}, {6.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -6.0f}, 0));
        quads.push_back(makeQuad({-3.0f,  3.0f, -3.0f}, {6.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -6.0f}, 0));
        quads.push_back(makeQuad({-3.0f, -3.0f, -9.0f}, {6.0f, 0.0f, 0.0f}, {0.0f, 6.0f,  0.0f}, 0));
        quads.push_back(makeQuad({-3.0f, -3.0f, -3.0f}, {0.0f, 6.0f, 0.0f}, {0.0f, 0.0f, -6.0f}, 1));
        quads.push_back(makeQuad({ 3.0f, -3.0f, -3.0f}, {0.0f, 6.0f, 0.0f}, {0.0f, 0.0f, -6.0f}, 2));
        quads.push_back(makeQuad({-1.2f, 2.99f, -5.0f}, {2.4f, 0.0f, 0.0f}, {0.0f, 0.0f, -2.4f}, 3));

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(0.0f, -1.0f, -6.5f));
        transform = glm::rotate(transform, glm::radians(90.0f), glm::vec3(0,1,0));
        transform = glm::scale(transform, glm::vec3(5.0f));
        dragonMesh.appendToScene(vertices, indices, 4, transform);

        useHDRI = 0;
        background_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        cam.Position = glm::vec3(0.0f, -0.5f, 0.0f);

        rebuildBVH();
        uploadAllBuffers();
        cam.moved = true;
    };

    // SCENE 4: Material lineup
    auto material_lineup = [&]() {
        clearScene();
        materials.push_back({glm::vec4(0.8f, 0.8f, 0.8f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.75f, 0.65f, 0.55f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.95f, 0.95f, 0.97f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(0.85f, 0.7f, 0.3f, 1.0f), glm::vec4(0.3f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f)});
        materials.push_back({glm::vec4(1.0f, 1.0f, 1.0f, 2.0f), glm::vec4(0.0f, 1.5f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)});
        materials.push_back({glm::vec4(1.0f, 1.0f, 1.0f, 2.0f), glm::vec4(0.0f, 1.5f, 0.0f, 0.0f), glm::vec4(1.5f, 1.0f, 0.1f, 0.0f)});

        quads.push_back(makeQuad(
            glm::vec3(-50.0f, -1.5f, -50.0f),
            glm::vec3(100.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 100.0f),
            0
        ));

        float radius = 0.8f;
        float y = -0.7f;
        float z = -5.0f;
        float spacing = 2.0f;

        spheres.push_back(makeSphere(glm::vec3(-2.0f * spacing, y, z), radius, 1));
        spheres.push_back(makeSphere(glm::vec3(-1.0f * spacing, y, z), radius, 2));
        spheres.push_back(makeSphere(glm::vec3( 0.0f * spacing, y, z), radius, 3));
        spheres.push_back(makeSphere(glm::vec3( 1.0f * spacing, y, z), radius, 4));
        spheres.push_back(makeSphere(glm::vec3( 2.0f * spacing, y, z), radius, 5));

        useHDRI = 1;
        envIntensity = 1.0f;
        background_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        cam.Position = glm::vec3(0.0f, 0.0f, 0.0f);

        rebuildBVH();
        uploadAllBuffers();
        cam.moved = true;
    };

    // SCENE 5: Breakfast room
    auto breakfast_room = [&](){
        clearScene();
        materials.push_back({glm::vec4(0.8f, 0.8f, 0.8f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::scale(transform, glm::vec3(25.0f));
        breakfastMesh.appendToScene(vertices, indices, 0, transform);

        cam.Position = glm::vec3(0.0f);
        useHDRI = 1;

        rebuildBVH();
        uploadAllBuffers();
        cam.moved = true;
    };

    // ===============================================GUI LAMBDA========================================================

    // Renders all ImGui windows. Returns whether the material buffer needs to be re-uploaded.
    auto renderGUI = [&]() {
        static const char* materialTypeNames[] = {
            "Lambertian", "Metal", "Dielectric", "Emissive", "Medium"
        };
        static const char* editableTypes[] = {
            "Lambertian", "Metal", "Dielectric", "Emissive"
        };

        bool needsMaterialUpload = false;

        // === World / Camera / Render controls ===
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340, 380), ImGuiCond_FirstUseEver);
        ImGui::Begin("World & Camera");

        ImGui::Text("Frame: %u   FPS: %.1f", frames, ImGui::GetIO().Framerate);
        ImGui::TextWrapped("Press TAB to switch between GUI and camera mode.");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Exposure", &exposure, 0.1f, 5.0f, "%.2f");

            if (ImGui::SliderFloat("Env Intensity", &envIntensity, 0.0f, 5.0f, "%.2f")) {
                cam.moved = true;
            }

            bool useHDRIbool = (useHDRI != 0);
            if (ImGui::Checkbox("Use HDRI", &useHDRIbool)) {
                useHDRI = useHDRIbool ? 1 : 0;
                cam.moved = true;
            }
            if (!useHDRIbool) {
                if (ImGui::ColorEdit3("Background", &background_color.x)) {
                    cam.moved = true;
                }
            }
        }

        if (ImGui::CollapsingHeader("Camera")) {
            if (ImGui::SliderFloat("FOV", &cam.Zoom, 10.0f, 120.0f, "%.1f deg")) {
                cam.moved = true;
            }
            if (ImGui::SliderFloat("Defocus angle", &defocus_angle, 0.0f, 10.0f, "%.2f")) {
                cam.moved = true;
            }
            if (ImGui::SliderFloat("Focus distance", &focus_dist, 0.1f, 30.0f, "%.2f")) {
                cam.moved = true;
            }
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", cam.Position.x, cam.Position.y, cam.Position.z);
            ImGui::Text("Move speed:");
            ImGui::SliderFloat("##speed", &cam.MovementSpeed, 0.5f, 20.0f, "%.1f");
        }

        ImGui::Separator();
        if (ImGui::Button("Save PNG", ImVec2(-FLT_MIN, 0))) {
            saveRequested = true;
        }
        if (ImGui::Button("Reset Accumulation", ImVec2(-FLT_MIN, 0))) {
            cam.moved = true;
        }

        ImGui::End();

        // === Scenes panel ===
        ImGui::SetNextWindowPos(ImVec2(10, 400), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340, 220), ImGuiCond_FirstUseEver);
        ImGui::Begin("Scenes");
        ImGui::TextWrapped("Switching a scene rebuilds the BVH and re-uploads all buffers.");
        ImGui::Separator();
        if (ImGui::Button("1. Scattered Spheres", ImVec2(-FLT_MIN, 0))) scattered_spheres();
        if (ImGui::Button("2. Glass Bunny",       ImVec2(-FLT_MIN, 0))) glass_bunny();
        if (ImGui::Button("3. Glass Dragon",      ImVec2(-FLT_MIN, 0))) glass_dragon();
        if (ImGui::Button("4. Material Lineup",   ImVec2(-FLT_MIN, 0))) material_lineup();
        if (ImGui::Button("5. Breakfast Room",    ImVec2(-FLT_MIN, 0))) breakfast_room();
        ImGui::End();

        // === Materials panel ===
        ImGui::SetNextWindowPos(ImVec2(360, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360, 610), ImGuiCond_FirstUseEver);
        ImGui::Begin("Materials");

        ImGui::TextWrapped("Selected material is highlighted in orange in the viewport.");
        ImGui::Spacing();

        if (ImGui::Button("Clear selection", ImVec2(-FLT_MIN, 0))) {
            selectedMaterial = -1;
            cam.moved = true;  // need to reset accumulation since the tint disappears
        }

        ImGui::Separator();
        ImGui::Text("Materials in scene:");

        if (ImGui::BeginListBox("##matlist", ImVec2(-FLT_MIN, 10 * ImGui::GetTextLineHeightWithSpacing()))) {
            for (int i = 0; i < (int)materials.size(); i++) {
                int type = int(materials[i].albedo.w);
                const char* typeName = (type >= 0 && type < 5) ? materialTypeNames[type] : "Unknown";

                // Color swatch
                ImVec4 swatch(materials[i].albedo.x, materials[i].albedo.y, materials[i].albedo.z, 1.0f);
                ImGui::ColorButton(("##sw" + std::to_string(i)).c_str(), swatch,
                                   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoInputs,
                                   ImVec2(16, 16));
                ImGui::SameLine();

                char label[128];
                snprintf(label, sizeof(label), "Material %d (%s)", i, typeName);

                if (ImGui::Selectable(label, selectedMaterial == i)) {
                    selectedMaterial = i;
                    cam.moved = true;  // refresh accumulation so the tint shows up cleanly
                }
            }
            ImGui::EndListBox();
        }

        // === Material editor for selected one ===
        if (selectedMaterial >= 0 && selectedMaterial < (int)materials.size()) {
            Material& m = materials[selectedMaterial];

            ImGui::Separator();
            ImGui::Text("Editing Material %d", selectedMaterial);
            ImGui::Spacing();

            bool changed = false;
            int currentType = int(m.albedo.w);

            if (currentType == 4) {
                // Medium materials are configured via the medium primitive, not edited here
                ImGui::TextWrapped("Medium materials are read-only here. They are configured by the medium primitive itself.");
                ImGui::Spacing();
                ImGui::ColorEdit3("Albedo (read-only)", &m.albedo.x, ImGuiColorEditFlags_NoInputs);
            } else {
                int typeIdx = (currentType >= 0 && currentType <= 3) ? currentType : 0;
                if (ImGui::Combo("Type", &typeIdx, editableTypes, 4)) {
                    m.albedo.w = float(typeIdx);
                    currentType = typeIdx;
                    // Provide sensible defaults when switching into a new type
                    if (currentType == 3 && m.extra.z == 0.0f) m.extra.z = 5.0f;  // emissive intensity
                    if (currentType == 2 && m.extra.y == 0.0f) m.extra.y = 1.5f;  // IOR
                    changed = true;
                }

                if (currentType != 2) {  // Hide albedo for dielectrics
                    if (ImGui::ColorEdit3("Albedo", &m.albedo.x)) changed = true;
                }

                switch (currentType) {
                    case 1:  // Metal
                        if (ImGui::SliderFloat("Fuzz", &m.extra.x, 0.0f, 1.0f, "%.3f")) changed = true;
                        break;
                    case 2:  // Dielectric
                        if (ImGui::SliderFloat("IOR", &m.extra.y, 1.0f, 2.5f, "%.3f")) changed = true;
                        ImGui::Text("Absorption (Beer-Lambert):");
                        if (ImGui::SliderFloat("R##abs", &m.absorption.x, 0.0f, 3.0f, "%.2f")) changed = true;
                        if (ImGui::SliderFloat("G##abs", &m.absorption.y, 0.0f, 3.0f, "%.2f")) changed = true;
                        if (ImGui::SliderFloat("B##abs", &m.absorption.z, 0.0f, 3.0f, "%.2f")) changed = true;
                        break;
                    case 3:  // Emissive
                        if (ImGui::SliderFloat("Intensity", &m.extra.z, 0.0f, 50.0f, "%.2f")) changed = true;
                        break;
                    // Lambertian (0) has no extra controls
                }
            }

            if (changed) {
                needsMaterialUpload = true;
                cam.moved = true;
            }
        }

        ImGui::End();

        return needsMaterialUpload;
    };

    //INITIALISE THE DEFAULT SCENE
    scattered_spheres();

    Shader displayShader = Shader("src\\display.vs", "src\\display.fs", false);
    Shader tracerShader = Shader("src\\tracer.vs", "src\\tracer.fs", false);

    //Main Render Loop
    while (!glfwWindowShouldClose(window)) {
        double currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        process_input(window);

        // === ImGui new frame ===
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Only show GUI when in GUI mode — keeps the camera-mode view clean
        bool needsMaterialUpload = false;
        if (guiMode) {
            needsMaterialUpload = renderGUI();
        }

        if (needsMaterialUpload) {
            uploadMaterials();
        }

        if(cam.moved){
            glBindFramebuffer(GL_FRAMEBUFFER, ping_pong_buffer[0]);
            glViewport(0, 0, WIDTH, HEIGHT);
            glClearColor(0.0f, 0.0f, 0.0f,1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebuffer(GL_FRAMEBUFFER, ping_pong_buffer[1]);
            glViewport(0, 0, WIDTH, HEIGHT);
            glClearColor(0.0f, 0.0f, 0.0f,1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            frames = 1u;
            cam.moved = false;
        }else{
            frames += 1u;
        }

        //First we create the image on the Ping Pong Buffer
        glBindVertexArray(Screen_Quad_VAO);
        glBindFramebuffer(GL_FRAMEBUFFER, ping_pong_buffer[curr_write_buffer]);
        glViewport(0, 0, WIDTH, HEIGHT);
        tracerShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ping_pong_texture[1 - curr_write_buffer]);
        glUniform1i(glGetUniformLocation(tracerShader.ID, "prevFrameTexture"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textures.id());
        glUniform1i(glGetUniformLocation(tracerShader.ID, "textures"), 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, hdriTexture);
        glUniform1i(glGetUniformLocation(tracerShader.ID, "envMap"), 2);
        glUniform1f(glGetUniformLocation(tracerShader.ID, "envIntensity"), envIntensity);

        // === Pass the highlight selection to the shader ===
        // The shader uses this to tint primitives that use the selected material.
        // -1 means "no highlight". Only highlight when GUI is up to avoid tinting renders we save.
        int highlightMat = (guiMode ? selectedMaterial : -1);
        glUniform1i(glGetUniformLocation(tracerShader.ID, "highlightMaterial"), highlightMat);

        //Updating the camera data
        camData.camPosition = glm::vec4(cam.Position,1.0f);
        camData.cameraRight = glm::vec4(cam.Right, 0.0f);
        camData.cameraUp = glm::vec4(cam.Up, 0.0f);
        camData.cameraForward = glm::vec4(cam.Front, 0.0f);
        camData.screenData.x = WIDTH;
        camData.screenData.y = HEIGHT;
        camData.cameraData.x = glm::radians(cam.Zoom);
        camData.screenData.z = frames;
        camData.screenData.w = useHDRI;
        camData.cameraData.y = glm::radians(defocus_angle);
        camData.cameraData.z = focus_dist;
        camData.backGround_color = background_color;
        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBO),&camData);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        //Now we sample the image on the Display Buffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.0f, 0.0f, 0.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        displayShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ping_pong_texture[curr_write_buffer]);
        glUniform1i(glGetUniformLocation(displayShader.ID, "displayTexture"), 0);
        glUniform1f(glGetUniformLocation(displayShader.ID, "exposure"), exposure);
        glBindVertexArray(Screen_Quad_VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        if (saveRequested)
        {
            saveFramebufferToPNG(
                ping_pong_texture[curr_write_buffer],
                WIDTH,
                HEIGHT,
                "render_" + std::to_string(frames) + ".png"
            );
            saveRequested = false;
        }

        // === Render ImGui on top of everything ===
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        curr_write_buffer = 1 - curr_write_buffer;
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // === ImGui cleanup ===
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    WIDTH = width;
    HEIGHT = height;
    glViewport(0, 0, width, height);
}

void process_input(GLFWwindow* window){
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
        glfwSetWindowShouldClose(window,true);
    }

    // Tab toggles GUI mode
    static bool tabPressed = false;
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        if (!tabPressed) {
            guiMode = !guiMode;
            glfwSetInputMode(window, GLFW_CURSOR,
                             guiMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            firstMouse = true;  // prevent camera jump when returning to camera mode
            cam.moved = true;   // refresh accumulation when highlight toggles
            std::cout << (guiMode ? "GUI mode" : "Camera mode") << std::endl;
        }
        tabPressed = true;
    } else {
        tabPressed = false;
    }

    // Skip movement controls while in GUI mode
    if (guiMode) {
        // Still allow P key to save in GUI mode
        static bool pPressedGui = false;
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            if (!pPressedGui) { saveRequested = true; pPressedGui = true; }
        } else pPressedGui = false;
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.ProcessKeyboard(FORWARD,  deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.ProcessKeyboard(LEFT,     deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.ProcessKeyboard(RIGHT,    deltaTime);
    if ((glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) && (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)){
        exposure *= 1.02f;
        if (exposure > 5.0f) exposure = 5.0f;
        std::cout<< "Exposure set to: " << exposure << std::endl;
    }
    if ((glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) && (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)){
        exposure *= 0.98f;
        if (exposure < 0.2f) exposure = 0.2f;
        std::cout<< "Exposure set to: " << exposure << std::endl;
    }
    static bool pPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pPressed) { saveRequested = true; pPressed = true; }
    } else pPressed = false;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos){
    // Don't move camera while GUI mode is active
    if (guiMode) return;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    cam.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (guiMode) return;
    cam.ProcessMouseScroll(yoffset);
}

void saveFramebufferToPNG(GLuint texture, int width, int height, const std::string& filename)
{
    glBindTexture(GL_TEXTURE_2D, texture);

    std::vector<float> pixels(width * height * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());

    std::vector<unsigned char> image(width * height * 3);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int src = ((height - 1 - y) * width + x) * 4;
            int dst = (y * width + x) * 3;

            glm::vec3 c(pixels[src + 0], pixels[src + 1], pixels[src + 2]);

            auto aces = [](glm::vec3 c) {
                const float a = 2.51f, b = 0.03f, cc = 2.43f, d = 0.59f, e = 0.14f;
                return glm::clamp((c * (a * c + b)) / (c * (cc * c + d) + e), glm::vec3(0.0f), glm::vec3(1.0f));
            };

            c *= exposure;
            c = aces(c);
            c = glm::pow(c, glm::vec3(1.0f / 2.2f));

            image[dst + 0] = (unsigned char)(c.r * 255.0f);
            image[dst + 1] = (unsigned char)(c.g * 255.0f);
            image[dst + 2] = (unsigned char)(c.b * 255.0f);
        }
    }

    stbi_write_png(filename.c_str(), width, height, 3, image.data(), width * 3);
    std::cout << "Saved image: " << filename << std::endl;
}