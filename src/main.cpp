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

int WIDTH = 1920, HEIGHT = 1080;

glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  0.0f);
glm::vec3 WorldUp    = glm::vec3(0.0f, 1.0f,  0.0f);

//for delta time
float lastFrame = 0.0f;
float deltaTime = 0.0f;
//for mouse position
float lastX = 400, lastY = 300;
//for mouse rotation
float yaw = -90.0f, pitch = 0.0f;
//for checking if this is the first time we recieve mouse input after coming into focus
bool firstMouse = true;
//Count of frames
unsigned int frames = 1;
//Depth of Field
float defocus_angle = 0.0f;
float focus_dist = 3.4f;
//For saving images
bool saveRequested = false;
//For Rendering
glm::vec4 background_color = glm::vec4(0.02f, 0.02f, 0.03f, 1.0f);// xyz = color, w = intensity;
float exposure = 1.0f;
float envIntensity = 1.5f;

//Setting up the Camera
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
    //x = WIDTH
    //y = HEIGHT
    //z = frameCount
    //w = 0 -> Solid-Background, 1 -> HDRI
    glm::vec4 cameraData;
    //x = fov
    //y = defocus_angle
    //z = focus_dist
    //w = unused
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

    //======Setting up the framebuffers for playing ping-pong======
    unsigned int tracer;
    unsigned int display;
    glGenFramebuffers(1, &tracer);
    glGenFramebuffers(1, &display);

    //making textures for both the framebuffers
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

    //Attaching the textures to the framebuffers
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
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f,//top right
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,//top left
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,//bottom left
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f//bottom right
    };

    unsigned int screen_indices[] = {
        0, 3, 2,
        0, 2, 1
    };

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

    //Making the World Objects here (Spheres only for now)
    
    std::vector<Material> materials;
    std::vector<GPUSphere> spheres;
    std::vector<GPUQuad> quads;
    std::vector<GPUVertex> vertices;
    std::vector<GPUIndex> indices;
    std::vector<GPUMediumSphere> mediumSpheres;
    TextureArray textures(1024, 1024, 16);

    // ===============================================SCENE========================================================

    //=======THIS IS THE FINAL SCENE I RENDER :)===========

    // ============================================================
    // SCATTERED SPHERES WITH MIXED EMISSIVE LIGHTS
    // ============================================================

    materials.clear();
    spheres.clear();
    quads.clear();
    vertices.clear();
    indices.clear();
    mediumSpheres.clear();

    // ============================================================
    // MATERIALS - palette of colors for spheres and lights
    // ============================================================

    // --- Lambertian materials (matte spheres) ---
    // 0-7: various muted colors
    materials.push_back({glm::vec4(0.85f, 0.85f, 0.85f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});   // 0: white
    materials.push_back({glm::vec4(0.2f, 0.2f, 0.25f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});    // 1: dark gray
    materials.push_back({glm::vec4(0.5f, 0.15f, 0.6f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});    // 2: purple
    materials.push_back({glm::vec4(0.15f, 0.5f, 0.5f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});    // 3: teal
    materials.push_back({glm::vec4(0.6f, 0.2f, 0.2f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});     // 4: dark red
    materials.push_back({glm::vec4(0.2f, 0.4f, 0.6f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});     // 5: muted blue
    materials.push_back({glm::vec4(0.4f, 0.35f, 0.2f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});    // 6: olive
    materials.push_back({glm::vec4(0.3f, 0.3f, 0.3f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});     // 7: medium gray

    // --- Metal materials (polished spheres) ---
    materials.push_back({glm::vec4(0.9f, 0.9f, 0.92f, 1.0f), glm::vec4(0.05f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f)});  // 8: chrome
    materials.push_back({glm::vec4(0.8f, 0.7f, 0.4f, 1.0f), glm::vec4(0.1f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f)});    // 9: gold

    // --- Glass material ---
    materials.push_back({
        glm::vec4(1.0f, 1.0f, 1.0f, 2.0f),
        glm::vec4(0.0f, 1.5f, 0.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)
    });  // 10: clear glass

    // --- Emissive materials (bright lights) ---
    materials.push_back({glm::vec4(1.0f, 1.0f, 1.0f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});   // 11: white light
    materials.push_back({glm::vec4(0.3f, 1.0f, 0.3f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});   // 12: green light
    materials.push_back({glm::vec4(1.0f, 0.3f, 1.0f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});   // 13: magenta light
    materials.push_back({glm::vec4(0.3f, 0.5f, 1.0f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});   // 14: blue light
    materials.push_back({glm::vec4(1.0f, 0.9f, 0.3f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});   // 15: yellow light
    materials.push_back({glm::vec4(0.3f, 1.0f, 1.0f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});   // 16: cyan light
    materials.push_back({glm::vec4(1.0f, 0.5f, 0.2f, 3.0f), glm::vec4(0.0f, 0.0f, 20.0f, 0.0f), glm::vec4(0.0f)});   // 17: orange light

    // --- Dark floor ---
    materials.push_back({glm::vec4(0.08f, 0.08f, 0.1f, 0.0f), glm::vec4(0.0f), glm::vec4(0.0f)});  // 18: nearly-black floor

    // ============================================================
    // HELPERS
    // ============================================================

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

    // ============================================================
    // FLOOR
    // ============================================================

    quads.push_back(makeQuad(
        glm::vec3(-30.0f, -2.0f, -30.0f),
        glm::vec3(60.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 60.0f),
        18
    ));

    // ============================================================
    // SCATTERED SPHERES
    // ============================================================

    // Material pools — what kinds of spheres to generate
    // Roughly 60% Lambertian, 20% metal/glass, 20% emissive
    int lambertianMats[] = {0, 1, 2, 3, 4, 5, 6, 7};
    int specularMats[]   = {8, 9, 10};                      // chrome, gold, glass
    int emissiveMats[]   = {11, 12, 13, 14, 15, 16, 17};   // 7 light colors

    // Use std::mt19937 for reproducible randomness — fix a seed so renders are deterministic
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> distX(-4.0f, 4.0f);       // x range
    std::uniform_real_distribution<float> distZ(-9.0f, -3.0f);      // z range (negative because looking down -z)
    std::uniform_real_distribution<float> distRadius(0.08f, 0.5f);  // size range
    std::uniform_real_distribution<float> distMatPick(0.0f, 1.0f);  // material category roll

    int NUM_SPHERES = 150;  // start here, scale up if your laptop handles it

    for (int i = 0; i < NUM_SPHERES; i++) {
        float r = distRadius(rng);
        float roll = distMatPick(rng);
        
        int mat;
        if (roll < 0.6f) {
            // 60% Lambertian
            mat = lambertianMats[rng() % 8];
        } else if (roll < 0.8f) {
            // 20% specular (metal or glass)
            mat = specularMats[rng() % 3];
        } else {
            // 20% emissive
            mat = emissiveMats[rng() % 7];
        }
        
        // Place sphere with center.y = floor_y + r so it sits on the floor
        glm::vec3 center(distX(rng), -2.0f + r, distZ(rng));
        
        spheres.push_back(makeSphere(center, r, mat));
    }

    //================================================BVH GENERATION FOR THE SCENE=============================================
    //Generating the BVH for the Scene
    std::vector<PrimitiveRef> refs;
    aabb ab;
    for(int i = 0; i < spheres.size(); i++){
        refs.push_back({0, i, ab.sphere_aabb(spheres[i]), ab.sphere_centroid(spheres[i])});
    }
    for(int i = 0; i < quads.size(); i++){
        refs.push_back({1, i, ab.quad_aabb(quads[i]), ab.quad_centroid(quads[i])});
    }
    for(int i = 0; i < indices.size(); i++){
        refs.push_back({2, i, ab.triangle_aabb(indices[i], vertices), ab.triangle_centroid(indices[i], vertices)});
    }
    for (int i = 0; i < mediumSpheres.size(); i++) {
        refs.push_back({3, i, ab.medium_sphere_aabb(mediumSpheres[i]), ab.medium_sphere_centroid(mediumSpheres[i])});
    }

    bvh_node root(refs, 0, (int)refs.size());

    std::cout << "BVH built: " << root.count_nodes() << "nodes, depth " << root.max_depth() << "\n";   

    std::vector<GPUBVHNode> gpu_bvh = root.flatten();

    std::vector<GPUPrimitiveRef> gpu_refs;
    gpu_refs.reserve(refs.size());
    for (const auto& r: refs){
        GPUPrimitiveRef gr;
        gr.data = glm::ivec4(r.primitive_type, r.index, 0, 0);
        gpu_refs.push_back(gr);
    }
    //Passing the world objects using an SSBO
    unsigned int sphereBuffer, materialBuffer, quadBuffer, vertexBuffer, indexBuffer, bvhBuffer, primRefsBuffer, mediumSphereBuffer;
    glGenBuffers(1, &sphereBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(GPUSphere), spheres.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereBuffer);
    glGenBuffers(1, &materialBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(Material), materials.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, materialBuffer);
    glGenBuffers(1, &quadBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, quadBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, quads.size() * sizeof(GPUQuad), quads.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, quadBuffer);
    glGenBuffers(1, &vertexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(GPUVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, vertexBuffer);
    glGenBuffers(1, &indexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(GPUIndex), indices.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, indexBuffer);
    glGenBuffers(1, &bvhBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_bvh.size() * sizeof(GPUBVHNode), gpu_bvh.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, bvhBuffer);
    glGenBuffers(1, &primRefsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, primRefsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_refs.size() * sizeof(GPUPrimitiveRef), gpu_refs.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, primRefsBuffer);
    glGenBuffers(1, &mediumSphereBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mediumSphereBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, mediumSpheres.size() * sizeof(GPUMediumSphere), mediumSpheres.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, mediumSphereBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    Shader displayShader = Shader("src\\display.vs", "src\\display.fs", false);
    Shader tracerShader = Shader("src\\tracer.vs", "src\\tracer.fs", false);

    //Main Render Loop
    while (!glfwWindowShouldClose(window)) {
        //update the deltaTime
        double currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        //Handle Screen Input
        process_input(window);

        //In case the Camera Moved in the previous frame - we reset accumulation
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
        //Updating the camera data
        camData.camPosition = glm::vec4(cam.Position,1.0f);
        camData.cameraRight = glm::vec4(cam.Right, 0.0f);
        camData.cameraUp = glm::vec4(cam.Up, 0.0f);
        camData.cameraForward = glm::vec4(cam.Front, 0.0f);
        camData.screenData.x = WIDTH;
        camData.screenData.y = HEIGHT;
        camData.cameraData.x = glm::radians(cam.Zoom);
        camData.screenData.z = frames;
        camData.screenData.w = 0; //To set HDRI
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
        glUniform1f(glGetUniformLocation(displayShader.ID, "exposure"), exposure);//setting the exposure
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

        //flip the drawing texture for the next time
        curr_write_buffer = 1 - curr_write_buffer;
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

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

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        if (!pPressed)
        {
            saveRequested = true;
            pPressed = true;
        }
    }
    else
    {
        pPressed = false;
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos){

    if (firstMouse) // initially set to true
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates range from bottom to top
    lastX = xpos;
    lastY = ypos;

    cam.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    cam.ProcessMouseScroll(yoffset);
}

void saveFramebufferToPNG(GLuint texture, int width, int height, const std::string& filename)
{
    glBindTexture(GL_TEXTURE_2D, texture);

    // Read float RGBA pixels
    std::vector<float> pixels(width * height * 4);

    glGetTexImage(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        GL_FLOAT,
        pixels.data()
    );

    // Convert to 8-bit RGB
    std::vector<unsigned char> image(width * height * 3);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int src = ((height - 1 - y) * width + x) * 4;
            int dst = (y * width + x) * 3;

            glm::vec3 c(
                pixels[src + 0],
                pixels[src + 1],
                pixels[src + 2]
            );

            auto aces = [](glm::vec3 c) {
                const float a = 2.51f, b = 0.03f, cc = 2.43f, d = 0.59f, e = 0.14f;
                return glm::clamp((c * (a * c + b)) / (c * (cc * c + d) + e), glm::vec3(0.0f), glm::vec3(1.0f));
            };
            // ...
            c *= exposure;       // apply same exposure as display
            c = aces(c);          // ACES tonemap
            c = glm::pow(c, glm::vec3(1.0f / 2.2f));  // gamma

            image[dst + 0] = (unsigned char)(c.r * 255.0f);
            image[dst + 1] = (unsigned char)(c.g * 255.0f);
            image[dst + 2] = (unsigned char)(c.b * 255.0f);
        }
    }

    stbi_write_png(
        filename.c_str(),
        width,
        height,
        3,
        image.data(),
        width * 3
    );

    std::cout << "Saved image: " << filename << std::endl;
}