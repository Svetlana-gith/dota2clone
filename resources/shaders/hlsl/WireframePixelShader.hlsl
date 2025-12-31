// Wireframe pixel shader for terrain visualization

cbuffer LightingConstants : register(b1) {
    float4 lightDirection;
    float4 lightColor;
    float4 ambientColor;
    float4 cameraPosition;
    float4 materialParams;
};

cbuffer MaterialConstants : register(b2) {
    float4 baseColor_metallic;
    float4 emissive_roughness;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    // Simple wireframe color - bright green for visibility
    float3 wireframeColor = float3(0.0, 1.0, 0.0); // Bright green
    
    // Add some lighting for depth perception
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-lightDirection.xyz);
    float NdotL = max(dot(normal, lightDir), 0.3); // Minimum 0.3 for visibility
    
    float3 finalColor = wireframeColor * NdotL;
    
    return float4(finalColor, 1.0);
}