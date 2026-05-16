#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "shader.h"
#include "stb_image.h"
#include "camera.h"

int WIDTH = 800, HEIGHT = 600;

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

//Setting up the Camera
Camera cam = Camera(cameraPos, WorldUp, yaw, pitch); 

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

void process_input(GLFWwindow* window);

void mouse_callback(GLFWwindow* window, double xpos, double ypos);

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

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
    glm::vec2 _pad;
};

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Path Tracer", NULL, NULL);
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
    static_assert(sizeof(CameraUBO) == 96, "");
    CameraUBO camData{};
    unsigned int cameraUBO;
    glGenBuffers(1, &cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBO), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Making the World Objects here (Spheres only for now)
    struct Material {
        glm::vec4 albedo; // xyz = color, w = material type
        glm::vec4 extra; //x = fuzz, y = refraction index
    };
    static_assert(sizeof(Material) == 32, "");
    std::vector<Material> materials;

    struct GPUSphere {
        glm::vec4 center; //xyz = position, w = radius
        glm::vec4 extra; // x = material index
    };
    static_assert(sizeof(GPUSphere) == 32, "");
    std::vector<GPUSphere> spheres;

    struct GPUQuad {
        glm::vec4 Q; // xyz = Position, w = material index
        glm::vec4 u; // xyz = direction
        glm::vec4 v; //xyz = direction
    };
    static_assert(sizeof(GPUQuad) == 48, "");
    std::vector<GPUQuad> quads;

    // Materials (unchanged)
    materials.push_back({glm::vec4(0.65f, 0.05f, 0.05f, 0.0f), glm::vec4(0.0f)}); // 0: red wall
    materials.push_back({glm::vec4(0.12f, 0.45f, 0.15f, 0.0f), glm::vec4(0.0f)}); // 1: green wall
    materials.push_back({glm::vec4(0.73f, 0.73f, 0.73f, 0.0f), glm::vec4(0.0f)}); // 2: white walls/floor/ceiling
    materials.push_back({glm::vec4(0.9960f, 0.3242f, 0.2851f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)}); // 3: metal
    materials.push_back({glm::vec4(1.00f, 1.00f, 1.00f, 2.0f), glm::vec4(0.0f, 1.5f, 0.0f, 0.0f)}); // 4: glass

    auto makeQuad = [](glm::vec3 Q, glm::vec3 u, glm::vec3 v, int mat) {
        GPUQuad q;
        q.Q = glm::vec4(Q, float(mat));
        q.u = glm::vec4(u, 0.0f);
        q.v = glm::vec4(v, 0.0f);
        return q;
    };

    // Box occupies x ∈ [-2.5, 2.5], y ∈ [-2.5, 2.5], z ∈ [3, 8]
    // Camera at origin, looking +z. Front face at z=3 is OPEN (camera looks in through it).
    // All other 5 faces are closed.

    quads.push_back(makeQuad({ 2.5f, -2.5f, 3.0f}, {0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 5.0f}, 1)); // green right wall (x = +2.5)
    quads.push_back(makeQuad({-2.5f, -2.5f, 3.0f}, {0.0f, 5.0f, 0.0f}, {0.0f, 0.0f, 5.0f}, 0)); // red left wall   (x = -2.5)
    quads.push_back(makeQuad({-2.5f, -2.5f, 3.0f}, {5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 5.0f}, 2)); // white floor     (y = -2.5)
    quads.push_back(makeQuad({-2.5f,  2.5f, 3.0f}, {5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 5.0f}, 2)); // white ceiling   (y = +2.5)
    quads.push_back(makeQuad({-2.5f, -2.5f, 8.0f}, {5.0f, 0.0f, 0.0f}, {0.0f, 5.0f, 0.0f}, 2)); // white back wall (z = +8)

    auto makeSphere = [](glm::vec3 center, float radius, int mat) {
        GPUSphere s;
        s.center = glm::vec4(center, radius);
        s.extra  = glm::vec4(float(mat), 0.0f, 0.0f, 0.0f);
        return s;
    };

    // Metal sphere — back-left, larger, sitting on floor
    spheres.push_back(makeSphere(glm::vec3(-1.0f, -1.7f, 6.0f), 0.8f, 3));

    // Glass sphere — front-right, smaller, sitting on floor
    spheres.push_back(makeSphere(glm::vec3( 1.0f, -1.9f, 4.5f), 0.6f, 4));

    
    //Passing the world objects using an SSBO
    unsigned int sphereBuffer, materialBuffer, quadBuffer;
    glGenBuffers(1, &sphereBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(GPUSphere), spheres.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereBuffer);
    glGenBuffers(1, &materialBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, materialBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, materials.size() * sizeof(GPUSphere), materials.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, materialBuffer);
    glGenBuffers(1, &quadBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, quadBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, quads.size() * sizeof(GPUSphere), quads.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, quadBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BLOCK, 0);

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