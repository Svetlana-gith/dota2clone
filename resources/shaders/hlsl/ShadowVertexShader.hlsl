// Shadow pass vertex shader - renders depth from light's perspective

cbuffer PerObjectConstants : register(b0) {
    float4x4 worldMatrix;
    float4x4 viewProjMatrix; // This will be light's view-proj matrix for shadow pass
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input) {
    VSOutput output;
    
    // Transform to world space
    float4 worldPos = mul(worldMatrix, float4(input.position, 1.0f));
    
    // Transform to light's clip space
    output.position = mul(viewProjMatrix, worldPos);
    
    return output;
}