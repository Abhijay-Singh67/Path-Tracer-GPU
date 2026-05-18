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
    int   WIDTH;
    int   HEIGHT;
    float fov;
    unsigned int frameCount;
    float defocus_angle;
    float focus_dist;
    glm::vec4 backGround_color;
    float _pad;
    float __pad;
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

    //Making the World Objects here (Spheres only for now)
    
    std::vector<Material> materials;
    std::vector<GPUSphere> spheres;
    std::vector<GPUQuad> quads;
    std::vector<GPUVertex> vertices;
    std::vector<GPUIndex> indices;
    std::vector<GPUMediumSphere> mediumSpheres;
    TextureArray textures(1024, 1024, 16);

    // ===============================================SCENE========================================================
    // ============================================================
    // SCALED-DOWN FINAL SCENE (RTIOW) ADAPTED TO YOUR FRAMEWORK
    // ============================================================

    materials.clear();
    quads.clear();
    spheres.clear();
    mediumSpheres.clear();

    const float S = 0.05f;

    // ============================================================
    // RANDOM
    // ============================================================

    // ============================================================
    // MATERIALS
    // ============================================================

    // 0 --- green ground boxes
    materials.push_back({
        glm::vec4(0.48f, 0.83f, 0.53f, 0.0f),
        glm::vec4(0.0f),
        glm::vec4(0.0f)
    });

    // 1 --- emissive quad light
    materials.push_back({
        glm::vec4(1.0f, 1.0f, 1.0f, 3.0f),
        glm::vec4(0.0f, 0.0f, 7.0f, 0.0f),
        glm::vec4(0.0f)
    });

    // 2 --- diffuse sphere
    materials.push_back({
        glm::vec4(0.7f, 0.3f, 0.1f, 0.0f),
        glm::vec4(0.0f),
        glm::vec4(0.0f)
    });

    // 3 --- glass
    materials.push_back({
        glm::vec4(1.0f, 1.0f, 1.0f, 2.0f),
        glm::vec4(0.0f, 1.5f, 0.0f, 0.0f),
        glm::vec4(0.0f)
    });

    // 4 --- metal
    materials.push_back({
        glm::vec4(0.8f, 0.8f, 0.9f, 1.0f),
        glm::vec4(0.2f, 0.0f, 0.0f, 0.0f),
        glm::vec4(0.0f)
    });

    // 5 --- earth material
    materials.push_back({
        glm::vec4(1.0f, 1.0f, 1.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
        glm::vec4(0.0f)
    });

    // 6 --- gray sphere
    materials.push_back({
        glm::vec4(0.5f, 0.5f, 0.5f, 0.0f),
        glm::vec4(0.0f),
        glm::vec4(0.0f)
    });

    // 7 --- white cluster spheres
    materials.push_back({
        glm::vec4(0.73f, 0.73f, 0.73f, 0.0f),
        glm::vec4(0.0f),
        glm::vec4(0.0f)
    });

    // ============================================================
    // TEXTURES
    // ============================================================

    int earth_layer = textures.load("src\\earthmap.jpg");

    // reserve texture layer 0 for "no texture"
    materials[5].extra.w = float(earth_layer);

    // ============================================================
    // HELPERS
    // ============================================================

    auto makeQuad = [](glm::vec3 Q, glm::vec3 u, glm::vec3 v, int mat)
    {
        GPUQuad q;
        q.Q = glm::vec4(Q, float(mat));
        q.u = glm::vec4(u, 0.0f);
        q.v = glm::vec4(v, 0.0f);
        return q;
    };

    auto makeSphere = [](glm::vec3 center, float radius, int mat)
    {
        GPUSphere s;
        s.center = glm::vec4(center, radius);
        s.extra = glm::vec4(float(mat), 0.0f, 0.0f, 0.0f);
        return s;
    };

    // ============================================================
    // GROUND BOX FIELD
    // ============================================================

    int boxes_per_side = 20;

    for (int i = 0; i < boxes_per_side; i++)
    {
        for (int j = 0; j < boxes_per_side; j++)
        {
            float w = 100.0f * S;

            float x0 = (-1000.0f * S) + i * w;
            float z0 = (-1000.0f * S) + j * w;

            float y0 = 0.0f;

            float x1 = x0 + w;
            float y1 = glm::linearRand(1.0f, 101.0f) * S;
            float z1 = z0 + w;

            // top
            quads.push_back(makeQuad(
                glm::vec3(x0, y1, z0),
                glm::vec3(w, 0, 0),
                glm::vec3(0, 0, w),
                0
            ));

            // front
            quads.push_back(makeQuad(
                glm::vec3(x0, y0, z0),
                glm::vec3(w, 0, 0),
                glm::vec3(0, y1 - y0, 0),
                0
            ));

            // left
            quads.push_back(makeQuad(
                glm::vec3(x0, y0, z0),
                glm::vec3(0, y1 - y0, 0),
                glm::vec3(0, 0, w),
                0
            ));
        }
    }

    // ============================================================
    // LIGHT
    // ============================================================

    quads.push_back(makeQuad(
        glm::vec3(123, 554, 147) * S,
        glm::vec3(300, 0, 0) * S,
        glm::vec3(0, 0, 265) * S,
        1
    ));

    // ============================================================
    // MAIN SPHERES
    // ============================================================

    // diffuse sphere
    spheres.push_back(makeSphere(
        glm::vec3(415, 400, 200) * S,
        50 * S,
        2
    ));

    // glass sphere
    spheres.push_back(makeSphere(
        glm::vec3(260, 150, 45) * S,
        50 * S,
        3
    ));

    // metal sphere
    spheres.push_back(makeSphere(
        glm::vec3(0, 150, 145) * S,
        50 * S,
        4
    ));

    // earth sphere
    spheres.push_back(makeSphere(
        glm::vec3(400, 200, 400) * S,
        100 * S,
        5
    ));

    // gray sphere
    spheres.push_back(makeSphere(
        glm::vec3(220, 280, 300) * S,
        80 * S,
        6
    ));

    // ============================================================
    // VOLUMETRIC GLASS SPHERE
    // ============================================================

    // visible boundary
    spheres.push_back(makeSphere(
        glm::vec3(360,150,145) * S,
        70 * S,
        3
    ));

    // blue fog
    {
        GPUMediumSphere fog;

        fog.center = glm::vec4(
            glm::vec3(360,150,145) * S,
            70 * S
        );

        fog.albedo_density = glm::vec4(
            0.2f,
            0.4f,
            0.9f,
            0.006f
        );

        mediumSpheres.push_back(fog);
    }

    // ============================================================
    // GLOBAL ATMOSPHERIC FOG
    // ============================================================

    {
        GPUMediumSphere fog;

        fog.center = glm::vec4(
            0,0,0,
            5000 * S
        );

        fog.albedo_density = glm::vec4(
            1.0f,
            1.0f,
            1.0f,
            0.00005f
        );

        mediumSpheres.push_back(fog);
    }

    // ============================================================
    // SPHERE CLUSTER
    // ============================================================

    for (int i = 0; i < 1000; i++)
    {
        glm::vec3 p(
            glm::linearRand(0.0f, 165.0f) * S,
            glm::linearRand(0.0f, 165.0f) * S,
            glm::linearRand(0.0f, 165.0f) * S
        );

        float angle = glm::radians(15.0f);

        float cs = cos(angle);
        float sn = sin(angle);

        glm::vec3 r;

        r.x = cs * p.x + sn * p.z;
        r.y = p.y;
        r.z = -sn * p.x + cs * p.z;

        r += glm::vec3(-100, 270, 395) * S;

        spheres.push_back(makeSphere(
            r,
            10.0f * S,
            7
        ));
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
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textures.id());
        glUniform1i(glGetUniformLocation(tracerShader.ID, "prevFrameTexture"), 0);
        //Updating the camera data
        camData.camPosition = glm::vec4(cam.Position,1.0f);
        camData.cameraRight = glm::vec4(cam.Right, 0.0f);
        camData.cameraUp = glm::vec4(cam.Up, 0.0f);
        camData.cameraForward = glm::vec4(cam.Front, 0.0f);
        camData.WIDTH = WIDTH;
        camData.HEIGHT = HEIGHT;
        camData.fov = glm::radians(cam.Zoom);
        camData.frameCount = frames;
        camData.defocus_angle = glm::radians(defocus_angle);
        camData.focus_dist = focus_dist;
        camData.backGround_color = glm::vec4(0.0f);
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

            // simple tonemap
            c = c / (c + glm::vec3(1.0f));

            // gamma correction
            c = glm::pow(c, glm::vec3(1.0f / 2.2f));

            c = glm::clamp(c, 0.0f, 1.0f);

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