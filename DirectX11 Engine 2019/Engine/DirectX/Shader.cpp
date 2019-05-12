#include "Shader.h"
#include "PolygonLayout.h"

#include <d3d11.h>
#include <D3Dcompiler.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

Shader::Shader() {
    pl = new PolygonLayout();
}

bool Shader::LoadFile(std::string fname, ShaderType type) {
    // Create shader
    HRESULT hr;
    ID3DBlob *ShaderBuffer; ShaderBuffer = 0;
    
    // Read blob
    hr = D3DReadFileToBlob(std::wstring(fname.begin(), fname.end()).c_str(), &ShaderBuffer);
    if( FAILED(hr) ) {
        std::cout << "Can't read shader to blob! (" << type << ")" << std::endl;
        return 1;
    }

    // Create shader
    switch( type ) {
        case Vertex  : hr = gDirectX->gDevice->CreateVertexShader(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), NULL, &sVertex);     break;
        case Pixel   : hr = gDirectX->gDevice->CreatePixelShader(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), NULL, &sPixel);       break;
        case Geometry: hr = gDirectX->gDevice->CreateGeometryShader(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), NULL, &sGeometry); break;
        case Hull    : hr = gDirectX->gDevice->CreateHullShader(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), NULL, &sHull);         break;
        case Domain  : hr = gDirectX->gDevice->CreateDomainShader(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), NULL, &sDomain);     break;
        case Compute : hr = gDirectX->gDevice->CreateComputeShader(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), NULL, &sCompute);   break;
    }

    if( FAILED(hr) ) {
        ShaderBuffer->Release();
        ShaderBuffer = 0;
        std::cout << "Can't create shader! (" << type << ")" << std::endl;
        return 1;
    }

    // Shader was compiled natively
    Type |= type;

    // Store blob for futher use
    bShader[(int)log2((int)type)] = ShaderBuffer;

    if( type == Vertex ) {
        CreatePolygonLayout();
    }

    // Done
    return 0;
}

void Shader::DeleteShaders() {
    if( (Type & Vertex   == Vertex  ) && (Linked & Vertex   == Vertex  ) ) { sVertex->Release(); }
    if( (Type & Pixel    == Pixel   ) && (Linked & Pixel    == Pixel   ) ) { sPixel->Release(); }
    if( (Type & Geometry == Geometry) && (Linked & Geometry == Geometry) ) { sGeometry->Release(); }
    if( (Type & Hull     == Hull    ) && (Linked & Hull     == Hull    ) ) { sHull->Release(); }
    if( (Type & Domain   == Domain  ) && (Linked & Domain   == Domain  ) ) { sDomain->Release(); }
    if( (Type & Compute  == Compute ) && (Linked & Compute  == Compute ) ) { sCompute->Release(); }

    pl->Release();
}

void Shader::ReleaseBlobs() {
    for( int i = 0; i < Count; i++ ) {
        char j = 1 << i;

        // If exists and not linked
        if( (Type & j == Type) && (Linked & j == 0) ) {
            bShader[i]->Release();
            bShader[i] = 0;
        }
    }
}

ID3DBlob* Shader::GetBlob(ShaderType type) {
    return bShader[(int)log2((int)type)];
}

void Shader::Bind() {
    pl->Bind();

    if( (Type & Vertex   == Vertex  ) ) { gDirectX->gContext->VSSetShader(sVertex, NULL, 0); }
    if( (Type & Pixel    == Pixel   ) ) { gDirectX->gContext->PSSetShader(sPixel, NULL, 0); }
    if( (Type & Geometry == Geometry) ) { gDirectX->gContext->GSSetShader(sGeometry, NULL, 0); }
    if( (Type & Hull     == Hull    ) ) { gDirectX->gContext->HSSetShader(sHull, NULL, 0); }
    if( (Type & Domain   == Domain  ) ) { gDirectX->gContext->DSSetShader(sDomain, NULL, 0); }
//  if( (Type & Compute  == Compute ) ) { gDirectX->gContext->CSSetShader(sCompute, NULL, 0); }
}

void Shader::Dispatch(UINT x, UINT y, UINT z) {
    if( Type & Compute == Compute ) {
        gDirectX->gContext->CSSetShader(sCompute, NULL, 0);
        gDirectX->gContext->Dispatch(x, y, z);
    }
}

HRESULT Shader::CreateInputLayoutDescFromVertexShaderSignature(ID3D11InputLayout ** pInputLayout, int * inputLayoutByteLength) {
    std::vector<D3D11_INPUT_ELEMENT_DESC> q;
    return CreateInputLayoutDescFromVertexShaderSignature(pInputLayout, inputLayoutByteLength, q);
}

// Slightly modified version of: 
// https://gist.github.com/Cody-Duncan/d85740563ceea99f6619
HRESULT Shader::CreateInputLayoutDescFromVertexShaderSignature(ID3D11InputLayout** pInputLayout, int* inputLayoutByteLength, std::vector<D3D11_INPUT_ELEMENT_DESC> elements) {
    ID3DBlob* pShaderBlob = bShader[0]; // Get vertex shader blob

    // Reflect shader info
    ID3D11ShaderReflection* pVertexShaderReflection = nullptr;
    HRESULT hr = S_OK;
    if( FAILED(D3DReflect(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&pVertexShaderReflection)) ) {
        return S_FALSE;
    }

    // get shader description
    D3D11_SHADER_DESC shaderDesc;
    pVertexShaderReflection->GetDesc(&shaderDesc);

    // Read input layout description from shader info
    unsigned int byteOffset = 0;
    std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayoutDesc;
    for( unsigned int i = 0; i < shaderDesc.InputParameters; ++i ) {
        D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
        pVertexShaderReflection->GetInputParameterDesc(i, &paramDesc);

        // create input element desc
        D3D11_INPUT_ELEMENT_DESC elementDesc;
        elementDesc.SemanticName = paramDesc.SemanticName;
        elementDesc.SemanticIndex = paramDesc.SemanticIndex;
        elementDesc.InputSlot = 0;
        elementDesc.AlignedByteOffset = byteOffset;
        elementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        elementDesc.InstanceDataStepRate = 0;

        // Skip some auto-generated semantics
        if( elementDesc.SemanticName == "SV_InstanceID" ) { continue; }
        //std::cout << elementDesc.SemanticName << std::endl;

        // determine DXGI format
        if( paramDesc.Mask == 1 ) {
            if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32 ) elementDesc.Format = DXGI_FORMAT_R32_UINT;
            else if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32 ) elementDesc.Format = DXGI_FORMAT_R32_SINT;
            else if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32 ) elementDesc.Format = DXGI_FORMAT_R32_FLOAT;
            byteOffset += 4;
        } else if( paramDesc.Mask <= 3 ) {
            if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32 ) elementDesc.Format = DXGI_FORMAT_R32G32_UINT;
            else if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32 ) elementDesc.Format = DXGI_FORMAT_R32G32_SINT;
            else if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32 ) elementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
            byteOffset += 8;
        } else if( paramDesc.Mask <= 7 ) {
            if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32 ) elementDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
            else if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32 ) elementDesc.Format = DXGI_FORMAT_R32G32B32_SINT;
            else if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32 ) elementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
            byteOffset += 12;
        } else if( paramDesc.Mask <= 15 ) {
            if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32 ) elementDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
            else if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32 ) elementDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
            else if( paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32 ) elementDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            byteOffset += 16;
        }

        // Save element desc
        inputLayoutDesc.push_back(elementDesc);
    }

    // 
    for( int i = 0; i < elements.size(); i++ ) {
        // Determine DXGI format
        switch( elements[i].Format ) {
            case DXGI_FORMAT_R32_UINT:
            case DXGI_FORMAT_R32_SINT:
            case DXGI_FORMAT_R32_FLOAT:
                byteOffset += 4;
                break;

            case DXGI_FORMAT_R32G32_UINT:
            case DXGI_FORMAT_R32G32_SINT:
            case DXGI_FORMAT_R32G32_FLOAT:
                byteOffset += 8;
                break;

            case DXGI_FORMAT_R32G32B32_UINT:
            case DXGI_FORMAT_R32G32B32_SINT:
            case DXGI_FORMAT_R32G32B32_FLOAT:
                byteOffset += 12;
                break;

            case DXGI_FORMAT_R32G32B32A32_UINT:
            case DXGI_FORMAT_R32G32B32A32_SINT:
            case DXGI_FORMAT_R32G32B32A32_FLOAT:
                byteOffset += 16;
                break;
        }

        // Save element desc
        inputLayoutDesc.push_back(elements[i]);
    }

    // Try to create Input Layout
    hr = gDirectX->gDevice->CreateInputLayout(&inputLayoutDesc[0], inputLayoutDesc.size(), pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), pInputLayout);

    if( FAILED(hr)) {
        // Failed
        std::cout << "Failed to create Input Layout!" << std::endl;
    }

    // Free allocation shader reflection memory
    pVertexShaderReflection->Release();

    // Record byte length
    *inputLayoutByteLength = byteOffset;

    return hr;
}

bool Shader::CreatePolygonLayout() {
    int tmp;
    ID3D11InputLayout *IL;

    if( FAILED(CreateInputLayoutDescFromVertexShaderSignature(&IL, &tmp)) ) {
        return true;
    }

    pl->Assign(IL);

    return false;
}

bool Shader::CreatePolygonLayout(std::vector<D3D11_INPUT_ELEMENT_DESC> elements) {
    int tmp;
    ID3D11InputLayout *IL;

    if( FAILED(CreateInputLayoutDescFromVertexShaderSignature(&IL, &tmp, elements)) ) {
        return true;
    }

    pl->Assign(IL);

    return false;
}
