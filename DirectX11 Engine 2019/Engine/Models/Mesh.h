#pragma once

#include "Engine/DirectX/Buffer.h"
#include "Engine/DirectX/IndexBuffer.h"
#include "Engine/DirectX/VertexBuffer.h"
#include "Engine/Vertices.h"

class Mesh: public DirectXChild {
private:
    VertexBuffer *vb;
    IndexBuffer *ib;

public:
    Mesh();
    void Bind();
    void Render();
    void Render(UINT num);
    void SetBuffer(VertexBuffer *inVB, IndexBuffer *inIB);
    void Release();
};