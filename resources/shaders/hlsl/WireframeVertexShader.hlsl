// Wireframe vertex shader for terrain visualization

cbuffer PerObjectConstants : register(b0) {
    float4x4 worldMatrix;
    float4x4 viewProjMatrix;
};

cbuffer LightingConstants : register(b1) {
    float4 lightDirection;
    float4 lightColor;
    float4 ambientColor;
    float4 cameraPosition;
    float4 materialParams;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    
    // Transform to world space
    float4 worldPos = mul(worldMatrix, float4(input.position, 1.0f));
    output.worldPos = worldPos.xyz;
    
    // Transform to clip space
    output.position = mul(viewProjMatrix, worldPos);
    
    // Transform normal to world space
    output.normal = normalize(mul((float3x3)worldMatrix, input.normal));
    
    // Pass through texture coordinates
    output.texCoord = input.texCoord;
    
    return output;
}