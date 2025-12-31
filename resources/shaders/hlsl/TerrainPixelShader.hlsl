// Terrain pixel shader with Phong lighting

// Constant buffers
cbuffer LightingConstants : register(b1) {
    float4 lightDirection;    // направление света
    float4 lightColor;        // цвет света
    float4 ambientColor;      // ambient освещение
    float4 cameraPosition;    // позиция камеры
    float4 materialParams;    // diffuse, specular, shininess, unused
};

cbuffer MaterialConstants : register(b2) {
    float4 baseColor_metallic;    // baseColor.rgb, metallic
    float4 emissive_roughness;    // emissiveColor.rgb, roughness
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 viewDir : VIEW_DIR;
};

float4 main(PSInput input) : SV_TARGET {
    // Normalize interpolated vectors
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(input.viewDir);
    float3 lightDir = normalize(-lightDirection.xyz);
    
    // Material properties
    float3 baseColor = baseColor_metallic.rgb;
    float diffuseStrength = materialParams.x;
    float specularStrength = materialParams.y;
    float shininess = materialParams.z;
    
    // Ambient lighting
    float3 ambient = ambientColor.rgb * baseColor;
    
    // Diffuse lighting (Lambert)
    float NdotL = max(dot(normal, lightDir), 0.0);
    float3 diffuse = lightColor.rgb * baseColor * NdotL * diffuseStrength;
    
    // Specular lighting (Phong)
    float3 reflectDir = reflect(-lightDir, normal);
    float RdotV = max(dot(reflectDir, viewDir), 0.0);
    float spec = pow(RdotV, shininess);
    float3 specular = lightColor.rgb * spec * specularStrength;
    
    // Combine lighting components
    float3 finalColor = ambient + diffuse + specular;
    
    // Add slight emissive for better visibility
    finalColor += emissive_roughness.rgb * 0.1;
    
    // Gamma correction (простая)
    finalColor = pow(finalColor, 1.0/2.2);
    
    return float4(finalColor, 1.0);
}