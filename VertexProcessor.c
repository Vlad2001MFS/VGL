#include "VertexProcessor.h"
#include "GLInternal.h"
#include "Rasterizer.h"
#include "Common.h"

#define VGL_VP_MAX_VERTICES 1024

static vglVertex gVertices[VGL_VP_MAX_VERTICES];
static size_t gVerticesCount = 0;

void vglVPAddVertex(const vglVertex *v) {
    VGL_ASSERT(gVerticesCount < VGL_VP_MAX_VERTICES);

    gVertices[gVerticesCount++] = *v;
}

void vglVPProcess() {
    vglVec2i vpPos = gCurrentState->viewport.min;
    vglVec2i vpSize;
    VGL_RECT_GET_SIZE(vpSize, gCurrentState->viewport);

    //vglMat4f vpMat;
    ///VGL_MAT4_SET_VIEWPORT(vpMat, vpPos.x, vpPos.y, vpSize.x, vpSize.y);

    vglMat4f mvp;
    VGL_MAT4_MUL(mvp, gCurrentState->projMat, gCurrentState->modelViewMat);
    for (size_t i = 0; i < gVerticesCount; i++) {
        vglVertex *v = gVertices + i;
        vglVec4f pos = { 0, 0, 0, 0 };
        VGL_MAT4_MUL_VEC4(pos, mvp, v->pos);
        VGL_VEC4_DIV_SCALAR(pos, pos, pos.w);
        //VGL_MAT4_MUL_VEC4(v->pos, vpMat, pos);
        VGL_VEC4_ADD_SCALAR(v->pos, pos, 1);
        VGL_VEC4_DIV_SCALAR(v->pos, v->pos, 2);
        v->pos.y = 1.0f - v->pos.y;
        VGL_VEC2_MUL(v->pos, v->pos, vpSize);
        VGL_VEC2_ADD(v->pos, v->pos, vpPos);
    }

    vglRSProcess();
    gVerticesCount = 0;
}

const vglVertex *vglVPGetVertices() {
    return gVertices;
}

size_t vglVPGetVerticesCount() {
    return gVerticesCount;
}