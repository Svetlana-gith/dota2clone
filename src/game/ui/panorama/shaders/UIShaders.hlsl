// UIShaders.hlsl - Basic UI rendering shaders for Panorama
// DirectX 12 implementation

// ============ Constant Buffers ============

cbuffer ScreenConstants : register(b0) {
    float2 screenSize;      // Screen dimensions
    float2 padding;
};

cbuffer TransformConstants : register(b1) {
    float4x4 transform;     // 2D transform matrix
    float opacity;          // Global opacity
    float3 padding2;
};

// ============ Structures ============

struct VSInput {
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

// ============ Vertex Shader ============

PSInput VSMain(VSInput input) {
    PSInput output;
    
    // Convert from screen space (0,0 top-left) to NDC (-1,1 bottom-left)
    float2 pos = input.position;
    pos.x = (pos.x / screenSize.x) * 2.0 - 1.0;
    pos.y = 1.0 - (pos.y / screenSize.y) * 2.0;
    
    output.position = float4(pos, 0.0, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    
    return output;
}

// ============ Pixel Shaders ============

// Solid color rendering (no texture)
float4 PSMainSolid(PSInput input) : SV_TARGET {
    return input.color * opacity;
}

// Textured rendering (for images and text)
Texture2D tex : register(t0);
SamplerState samplerState : register(s0);

float4 PSMainTextured(PSInput input) : SV_TARGET {
    float4 texColor = tex.Sample(samplerState, input.uv);
    return texColor * input.color * opacity;
}

// Gradient rendering (using vertex colors)
float4 PSMainGradient(PSInput input) : SV_TARGET {
    return input.color * opacity;
}

// Rounded rectangle with SDF
float4 PSMainRounded(PSInput input) : SV_TARGET {
    // TODO: Implement SDF-based rounded corners
    // For now, just return solid color
    return input.color * opacity;
}
