struct VSInput {
    float2 inPosition; // attribute specified per-vertex
    float3 inColor;
}

struct VSOutput {
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.color = input.inColor;
    output.pos = float4(input.inPosition, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target {
    return float4(vertIn.color, 1.0);
}

