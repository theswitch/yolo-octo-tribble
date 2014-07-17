#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <random>
#include <iostream>

static const int numVertices = 50;

const GLchar* vertexSource = R"(
#version 330 core

in vec2 position; // current vertex position
in vec2 velocity; // current vertex velocity

out vec2 newPos; // updated vertex position
out vec2 newVel; // updated vertex velocity

uniform vec2 source; // position of gravity source (cursor)
uniform float dt; // timestep

const float reflectLoss = 0.5; // velocity loss upon reflection

void main() {
    vec2 diff = source - position;
    float r2 = clamp(length(diff) * length(diff), 0.1, 1.0);
    newVel = velocity + dt*normalize(diff)/r2;
    newPos = position + dt*newVel;

    // reflect particles when they go through a wall
    if (newPos.x < -1.0 || newPos.x > 1.0)
        newVel = reflectLoss*reflect(newVel, vec2(1.0, 0.0));
    if (newPos.y < -1.0 || newPos.y > 1.0)
        newVel = reflectLoss*reflect(newVel, vec2(0.0, 1.0));

    gl_PointSize = 5.0;
    gl_Position = vec4(position, 0.0, 1.0);
})";

const GLchar* fragmentSource = R"(
#version 330 core

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1.0);
})";

const GLchar* screenVertexSource = R"(
#version 330 core

in vec2 position;
in vec2 texcoord;
out vec2 vTexCoord;

void main() {
    vTexCoord = texcoord;
    gl_Position = vec4(position, 0.0, 1.0);
})";

const GLchar* screenFragmentSource = R"(
#version 330 core

in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;
uniform sampler2D texFramebuffer;

void main() {
    outColor = texture(texFramebuffer, vTexCoord);
})";

// position, texcoord for screen quad to draw framebuffer to
const GLfloat screenQuad[] = {
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, 0.0f,
};

// element index array for screenQuad
const GLuint elements[] = {
    0, 1, 2,
    2, 3, 0,
};

int main(int argc, char** argv)
{
    glfwInit();

    // support at least OpenGL 3.2
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    // create a windowed window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Cursor Gravity", nullptr, nullptr);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    // activate the OpenGL context
    glfwMakeContextCurrent(window);

    // initialise GLEW
    glewExperimental = GL_TRUE;
    glewInit();

    std::vector<glm::vec4> vertices;
    std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // generate random initial positions for vertices, with 0 velocity
    for (int i = 0; i < numVertices; i++)
        vertices.push_back(glm::vec4(dist(generator), dist(generator), 0.0f, 0.0f));

    // create vertex array object
    GLuint vao[3];
    glGenVertexArrays(3, vao);

    // create vertex buffers and transform feedbacks
    GLuint vbo[3];
    glGenBuffers(3, vbo);

    // vbo with initial vertex data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec4),
            &vertices[0], GL_STREAM_DRAW);
    // vbo for transform feedback
    // we use GL_STREAM_COPY because data here changes every frame
    // and is only used by OpenGL
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec4),
            nullptr, GL_STREAM_COPY);
    // vbo with screen quad data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenQuad),
            screenQuad, GL_STATIC_DRAW);

    // create shaders
    // simulation/scene shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    // notify OpenGL of the things we need out of the transform feedback
    const GLchar* feedbackVaryings[] = { "newPos", "newVel" };
    glTransformFeedbackVaryings(shaderProgram, 2, feedbackVaryings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(shaderProgram);

    // screen shader
    GLuint screenVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(screenVertexShader, 1, &screenVertexSource, nullptr);
    glCompileShader(screenVertexShader);

    GLuint screenFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(screenFragmentShader, 1, &screenFragmentSource, nullptr);
    glCompileShader(screenFragmentShader);

    GLuint screenShaderProgram = glCreateProgram();
    glAttachShader(screenShaderProgram, screenVertexShader);
    glAttachShader(screenShaderProgram, screenFragmentShader);
    glLinkProgram(screenShaderProgram);

    // specify layout of vertex data for each vao
    for (int i = 0; i < 2; i++) {
        glBindVertexArray(vao[i]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);

        GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
        glEnableVertexAttribArray(posAttrib);
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE,
                4 * sizeof(float), 0);

        GLint velAttrib = glGetAttribLocation(shaderProgram, "velocity");
        glEnableVertexAttribArray(velAttrib);
        glVertexAttribPointer(velAttrib, 2, GL_FLOAT, GL_FALSE,
                4 * sizeof(float), (void*) (2 * sizeof(float)));
    }

    // vertex data for screen quad
    glBindVertexArray(vao[2]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);

    GLint posAttrib = glGetAttribLocation(screenShaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE,
            4 * sizeof(float), 0);

    GLint texAttrib = glGetAttribLocation(screenShaderProgram, "texcoord");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE,
            4 * sizeof(float), (void*) (2 * sizeof(float)));

    // element index buffer for screen quad
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

    // create framebuffer object to render to
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // texture to attach to framebuffer
    GLuint texColorBuffer;
    glGenTextures(1, &texColorBuffer);
    glBindTexture(GL_TEXTURE_2D, texColorBuffer);
    // specify storage of texture (but no copying from cpu)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB,
            GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, texColorBuffer, 0);

    // uniform locations for simulation
    GLint uniTime = glGetUniformLocation(shaderProgram, "dt");
    GLint uniSource = glGetUniformLocation(shaderProgram, "source");

    glUseProgram(screenShaderProgram);
    glUniform1i(glGetUniformLocation(screenShaderProgram, "texFramebuffer"), 0);

    // allow us to use gl_PointSize in the vertex shader to get bigger
    // pixels during rasterisation (we can just render our vertices as
    // points and not worry)
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    GLuint query;
    glGenQueries(1, &query);

    double prevTime = glfwGetTime();
    int currVB = 0, currTFB = 1;
    while (!glfwWindowShouldClose(window)) {
        // draw to framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glUseProgram(shaderProgram);
        glClear(GL_COLOR_BUFFER_BIT);

        double frameTime = glfwGetTime();
        double dt = frameTime - prevTime;
        //glUniform1f(uniTime, dt);
        glUniform1f(uniTime, 1e-1);

        // send cursor position to shader
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        //glUniform2f(uniSource, (x - 400.0f)/400.0f, (300.0f - y)/300.0f);
        glUniform2f(uniSource, 0.0f, 0.0f);

        // bind vertex array object
        glBindVertexArray(vao[currVB]);
        // bind the other buffer to receive transform feedback
        //glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo[currTFB]);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo[currTFB], 0, vertices.size() * sizeof(glm::vec4));

        GLint64 size;
        glGetInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, 0, &size);
        std::cout << "size: " << size << std::endl;

        // draw vertices, wrapped in transform feedback
        glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, query);
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, numVertices);
        glEndTransformFeedback();
        glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

        GLuint primitives;
        glGetQueryObjectuiv(query, GL_QUERY_RESULT, &primitives);
        std::cout << primitives << " primitives written" << std::endl;

        // draw to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(screenShaderProgram);
        glBindVertexArray(vao[2]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // swap vertex buffers
        currVB = currTFB;
        currTFB = currTFB ^ 1;
        prevTime = frameTime;

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // cleanup and terminate
    glDeleteQueries(1, &query);

    glDeleteProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glDeleteProgram(screenShaderProgram);
    glDeleteShader(screenVertexShader);
    glDeleteShader(screenFragmentShader);

    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &texColorBuffer);

    glDeleteVertexArrays(3, vao);
    glDeleteBuffers(3, vbo);
    glDeleteBuffers(1, &ebo);

    glfwTerminate();
    return 0;
}
