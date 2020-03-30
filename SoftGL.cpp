#include "SoftGL.hpp"
#include "glm/glm.hpp"
#include "glm/ext.hpp"
#include <vector>
#include <intrin.h>

// ***********************************************************************************************************
// ****************************************** SoftGL System Wrapper ******************************************
// ***********************************************************************************************************

struct {
    glm::ivec2 bufferSize, surfaceMin, surfaceMax;
    int colorBufferPitch;
    std::vector<uint32_t> colorBuffer;
    std::vector<float> depthBuffer;
} gSGL;

glm::u8vec4 *gColorBuffer = nullptr;
float *gDepthBuffer = nullptr;

void sglUpdateBuffers(int w, int h) {
    gSGL.bufferSize = glm::ivec2(w, h);
    gSGL.surfaceMin = glm::ivec2(0, 0);
    gSGL.surfaceMax = glm::ivec2(w - 1, h - 1);
    gSGL.colorBufferPitch = sizeof(uint32_t)*w;
    gSGL.colorBuffer.resize(w*h);
    gColorBuffer = reinterpret_cast<glm::u8vec4*>(gSGL.colorBuffer.data());

    gSGL.depthBuffer.resize(w*h);
    gDepthBuffer = gSGL.depthBuffer.data();
}

void sglGetColorBuffer(uint32_t **colorBuffer, int *pitch) {
    *colorBuffer = reinterpret_cast<glm::uint32_t*>(gColorBuffer);
    *pitch = gSGL.colorBufferPitch;
}

// ***********************************************************************************************************
// ****************************************** SoftGL Implementation ******************************************
// ***********************************************************************************************************

struct {
    glm::ivec2 pos = glm::ivec2(0, 0);
    glm::ivec2 size = glm::ivec2(0, 0);
    glm::ivec2 min = glm::ivec2(0, 0);
    glm::ivec2 max = glm::ivec2(0, 0);
} gVP;

int gMatrixMode = GL_MODELVIEW;
glm::mat4 gProjMat = glm::mat4(1.0f);
glm::mat4 gModelViewMat = glm::mat4(1.0f);

int gShadeModel = GL_SMOOTH;

uint32_t gClearColor = 0;
float gClearDepth = 1.0f;

bool gIsDepthTestEnabled = false;
int gDepthFunc = GL_LESS;

struct Vertex {
    glm::vec3 pos;
    glm::u8vec4 color;
};
int gImMode = 0;
std::vector<Vertex> gImVertices;
glm::u8vec4 gImCurrentColor = glm::u8vec4(255, 255, 255, 255);

__forceinline glm::mat4 &currentMat() {
    if (gMatrixMode == GL_MODELVIEW) {
        return gModelViewMat;
    }
    else if (gMatrixMode == GL_PROJECTION) {
        return gProjMat;
    }
}

__forceinline uint32_t floatColorToUint(float r, float g, float b, float a) {
    return *reinterpret_cast<const uint32_t*>(glm::value_ptr(glm::u8vec4(r*255, g*255, b*255, a*255)));
}

template<typename T>
__forceinline bool compareFunc(int func, T lhs, T rhs) {
    switch (func) {
        case GL_NEVER: {
            return false;
        }
        case GL_LESS: {
            return lhs < rhs;
        }
        case GL_EQUAL: {
            return glm::epsilonEqual(lhs, rhs, glm::epsilon<float>());
        }
        case GL_LEQUAL: {
            return lhs <= rhs;
        }
        case GL_GREATER: {
            return lhs > rhs;
        }
        case GL_NOTEQUAL: {
            return glm::epsilonNotEqual(lhs, rhs, glm::epsilon<float>());
        }
        case GL_GEQUAL: {
            return lhs >= rhs;
        }
        case GL_ALWAYS: {
            return true;
        }
    }
}

void drawTriangle(const Vertex &A, const Vertex &B, const Vertex &C) {
    const int ABx = B.pos.x - A.pos.x;
    const int ACx = C.pos.x - A.pos.x;

    const int ABy = B.pos.y - A.pos.y;
    const int ACy = C.pos.y - A.pos.y;

    const int w = ABx*ACy - ACx*ABy;
    if (glm::abs(w) >= 1.0f) {
        const float invW = 1.0f / w;

        const glm::ivec2 min = glm::clamp(glm::min<glm::ivec2>(A.pos, B.pos, C.pos), gVP.min, gVP.max);
        const glm::ivec2 max = glm::clamp(glm::max<glm::ivec2>(A.pos, B.pos, C.pos), gVP.min, gVP.max);

        for (int y = min.y; y <= max.y; y++) {
            const int PAy = A.pos.y - y;

            for (int x = min.x; x <= max.x; x++) {
                const int PAx = A.pos.x - x;

                const float bcScreenU = (ACx*PAy - PAx*ACy)*invW;
                const float bcScreenV = (PAx*ABy - ABx*PAy)*invW;
                const float bcScreenW = 1.0f - bcScreenU - bcScreenV;
                
                if (bcScreenU >= 0 && bcScreenV >= 0 && bcScreenW >= 0) {
                    float bcClipU = bcScreenU / B.pos.z;
                    float bcClipV = bcScreenV / C.pos.z;
                    float bcClipW = bcScreenW / A.pos.z;

                    const float bcClipUVW = bcClipU + bcClipV + bcClipW;
                    bcClipU /= bcClipUVW; // perspective-correct
                    bcClipV /= bcClipUVW;
                    bcClipW /= bcClipUVW;

                    const float depth = bcClipU*B.pos.z + bcClipV*C.pos.z + bcClipW*A.pos.z;

                    uint32_t idx = x + y*gSGL.bufferSize.x;
                    if (gIsDepthTestEnabled && compareFunc(gDepthFunc, depth, gDepthBuffer[idx])) {
                        glm::u8vec4 &color = gColorBuffer[idx];
                        color.r = (bcScreenU*B.color.r + bcScreenV*C.color.r + bcScreenW*A.color.r);
                        color.g = (bcScreenU*B.color.g + bcScreenV*C.color.g + bcScreenW*A.color.g);
                        color.b = (bcScreenU*B.color.b + bcScreenV*C.color.b + bcScreenW*A.color.b);
                        color.a = (bcScreenU*B.color.a + bcScreenV*C.color.a + bcScreenW*A.color.a);

                        gDepthBuffer[idx] = depth;
                    }
                    else if (!gIsDepthTestEnabled) {
                        glm::u8vec4 &color = gColorBuffer[idx];
                        color.r = (bcScreenU*B.color.r + bcScreenV*C.color.r + bcScreenW*A.color.r);
                        color.g = (bcScreenU*B.color.g + bcScreenV*C.color.g + bcScreenW*A.color.g);
                        color.b = (bcScreenU*B.color.b + bcScreenV*C.color.b + bcScreenW*A.color.b);
                        color.a = (bcScreenU*B.color.a + bcScreenV*C.color.a + bcScreenW*A.color.a);
                    }
                }
            }
        }
    }
}

void glViewport(int x, int y, int w, int h) {
    gVP.pos.x = x;
    gVP.pos.y = y;
    gVP.size.x = w;
    gVP.size.y = h;
    gVP.min = glm::max(gVP.pos, gSGL.surfaceMin);
    gVP.max = glm::min(gVP.pos + gVP.size - 1, gSGL.surfaceMax);
}

void glMatrixMode(int mode) {
    gMatrixMode = mode;
}

void glLoadIdentity() {
    currentMat() = glm::mat4(1.0f);
}

void glLoadMatrixf(const float *mat) {
    memcpy(glm::value_ptr(currentMat()), mat, sizeof(glm::mat4));
}

void glTranslatef(float x, float y, float z) {
    currentMat() *= glm::translate(glm::vec3(x, y, z));
}

void glRotatef(float angle, float x, float y, float z) {
    currentMat() *= glm::rotate(glm::radians(angle), glm::vec3(x, y, z));
}

void glScalef(float x, float y, float z) {
    currentMat() *= glm::scale(glm::vec3(x, y, z));
}

void glShadeModel(int mode) {
    gShadeModel = mode;
}

void glClearColor(float r, float g, float b, float a) {
    gClearColor = floatColorToUint(r, g, b, a);
}

void glClearDepth(float depth) {
    gClearDepth = depth;
}

void glClear(int mask) {
    if ((mask & GL_COLOR_BUFFER_BIT) == GL_COLOR_BUFFER_BIT) {
        for (int y = 0; y < gSGL.bufferSize.y; y++) {
            __stosd(reinterpret_cast<unsigned long*>(gColorBuffer + y*gSGL.bufferSize.x), gClearColor, gSGL.bufferSize.x);
        }
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) == GL_DEPTH_BUFFER_BIT) {
        for (int i = 0; i < gSGL.bufferSize.x*gSGL.bufferSize.y; i++) {
            gDepthBuffer[i] = gClearDepth;
        }
    }
}

void glEnable(int cap) {
    if ((cap & GL_DEPTH_TEST) == GL_DEPTH_TEST) {
        gIsDepthTestEnabled = true;
    }
}

void glDepthFunc(int func) {
    gDepthFunc = func;
}

void glBegin(int mode) {
    gImMode = mode;
    gImVertices.clear();
}

void glVertex3f(float x, float y, float z) {
    Vertex v;
    v.pos = glm::vec3(x, y, z);
    v.color = gImCurrentColor;
    gImVertices.push_back(v);
}

void glColor3f(float r, float g, float b) {
    gImCurrentColor = glm::u8vec4(r*255, g*255, b*255, 1.0f);
}

void glEnd() {
    glm::mat4 mvp = gProjMat*gModelViewMat;
    for (auto &vert : gImVertices) {
        glm::vec4 v = mvp*glm::vec4(vert.pos, 1.0f);
        vert.pos = glm::vec3(v) / v.w;
        vert.pos = (vert.pos + 1.0f) / 2.0f;
        vert.pos.y = 1.0f - vert.pos.y;
        vert.pos.x = static_cast<int>(gVP.pos.x + vert.pos.x*gVP.size.x);
        vert.pos.y = static_cast<int>(gVP.pos.y + vert.pos.y*gVP.size.y);
    }

    if (gImMode == GL_TRIANGLES) {
        for (int i = 0; i < gImVertices.size(); i += 3) {
            drawTriangle(gImVertices[i], gImVertices[i + 1], gImVertices[i + 2]);
        }
    }
    else if (gImMode == GL_QUADS) {
        for (int i = 0; i < gImVertices.size(); i += 4) {
            drawTriangle(gImVertices[i], gImVertices[i + 1], gImVertices[i + 2]);
            drawTriangle(gImVertices[i], gImVertices[i + 2], gImVertices[i + 3]);
        }
    }
}
