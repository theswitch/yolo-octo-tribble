#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <random>
#include <iostream>

static const int numVertices = 50;

const GLchar* vertexSource = R"(
#version 150

in vec2 position; // current vertex position
in vec2 velocity; // current vertex velocity

out vec2 newPos; // updated vertex position
out vec2 newVel; // updated vertex velocity

uniform vec2 source; // position of gravity source (cursor)
uniform float dt; // timestep

const float reflectLoss = 0.5;

void main() {
    vec2 diff = source - position;
    float r2 = clamp(length(diff) * length(diff), 0.1, 1.0);
    newVel = velocity + dt*normalize(diff)/r2;
    newPos = position + dt*newVel;

    if (newPos.x < -1.0 || newPos.x > 1.0)
        newVel = reflectLoss*reflect(newVel, vec2(1.0, 0.0));
    if (newPos.y < -1.0 || newPos.y > 1.0)
        newVel = reflectLoss*reflect(newVel, vec2(0.0, 1.0));

    gl_PointSize = 5.0;
    gl_Position = vec4(position, 0.0, 1.0);
})";

const GLchar* fragmentSource = R"(
#version 150

out vec4 outColor;

void main() {
    outColor = vec4(1.0);
})";

int main(int argc, char** argv)
{
    glfwInit();

    // support at least OpenGL 3.2
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
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
    std::default_random_engine generator; // random engine
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // generate random initial positions for vertices, with 0 velocity
    for (int i = 0; i < numVertices; i++)
        vertices.push_back(glm::vec4(dist(generator), dist(generator), 0.0f, 0.0f));

    // create vertex array object
    GLuint vao[2];
    glGenVertexArrays(2, vao);

    // create vertex buffers and transform feedbacks
    GLuint vbo[2];
    glGenBuffers(2, vbo);

    // vbo with initial vertex data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec4),
            &vertices[0], GL_DYNAMIC_DRAW);
    // vbo for transform feedback
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec4),
            nullptr, GL_DYNAMIC_DRAW);

    // create shaders
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
    glUseProgram(shaderProgram);

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

    GLint uniTime = glGetUniformLocation(shaderProgram, "dt");
    GLint uniSource = glGetUniformLocation(shaderProgram, "source");

    glEnable(GL_PROGRAM_POINT_SIZE);

    double prevTime = glfwGetTime();
    int currVB = 0, currTFB = 1;
    while (!glfwWindowShouldClose(window)) {
        double frameTime = glfwGetTime();
        double dt = frameTime - prevTime;
        glUniform1f(uniTime, dt);

        // send cursor position to shader
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        glUniform2f(uniSource, (x - 400.0)/400.0, (300.0 - y)/300.0);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // bind vertex array object
        glBindVertexArray(vao[currVB]);
        // bind buffer to draw from
        glBindBuffer(GL_ARRAY_BUFFER, vbo[currVB]);
        // bind the other buffer to receive transform feedback
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo[currTFB]);

        // draw vertices, wrapped in transform feedback
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, numVertices);
        glEndTransformFeedback();

        glfwSwapBuffers(window);
        glfwPollEvents();

        prevTime = frameTime;

        // swap vertex buffers
        currVB = currTFB;
        currTFB = currTFB ^ 1;
    }

    // cleanup and terminate
    glDeleteProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glDeleteVertexArrays(2, vao);
    glDeleteBuffers(2, vbo);

    glfwTerminate();
    return 0;
}
