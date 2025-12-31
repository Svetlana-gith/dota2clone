// Terrain vertex shader with lighting support

// Constant buffers
cbuffer PerObjectConstants : register(b0) {
    float4x4 worldMatrix;
    float4x4 viewProjMatrix;
};

cbuffer LightingConstants : register(b1) {
    float4 lightDirection;    // направление света
    float4 lightColor;        // цвет света
    float4 ambientColor;      // ambient освещение
    float4 cameraPosition;    // позиция камеры
    float4 materialParams;    // diffuse, specular, shininess, unused
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
    float3 viewDir : VIEW_DIR;
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
    
    // Calculate view direction
    output.viewDir = normalize(cameraPosition.xyz - worldPos.xyz);
    
    return output;
}