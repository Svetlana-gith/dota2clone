// Shadow pass pixel shader - just writes depth

struct PSInput {
    float4 position : SV_POSITION;
};

void main(PSInput input) {
    // Depth is automatically written to shadow map
    // No color output needed for shadow pass
}