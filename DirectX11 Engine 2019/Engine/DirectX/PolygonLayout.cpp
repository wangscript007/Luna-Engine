#include "pc.h"
#include "PolygonLayout.h"


ID3D11InputLayout* PolygonLayout::GetLayout() {
    return il;
}

void PolygonLayout::Begin() {

}

void PolygonLayout::Push(D3D11_INPUT_ELEMENT_DESC desc) {
    descs.push_back(desc);
}

bool PolygonLayout::End(Shader* shader) {
    UINT numEls = static_cast<UINT>(descs.size());
    D3D11_INPUT_ELEMENT_DESC *desc = (D3D11_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D11_INPUT_ELEMENT_DESC) * numEls);

    // Gather elements
    for( size_t i = 0; i < numEls; i++ ) desc[i] = descs[i];

    // Create input layout
    auto blob = shader->GetBlob(Shader::Vertex);
    return FAILED( gDirectX->gDevice->CreateInputLayout(desc, numEls, blob->GetBufferPointer(), blob->GetBufferSize(), &il) );
}

void PolygonLayout::Release() {
    if( Released ) return;
    if( il ) il->Release();
    Released = true;
}

void PolygonLayout::Bind() {
    if( !il ) { return; }
    gState = this;

    gDirectX->gContext->IASetInputLayout(il);
}

void PolygonLayout::Assign(ID3D11InputLayout* inIL) {
    il = inIL;
}
