static float2 positions[3] = float2[](
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
);

static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
);

struct vertexOutput {
    float4 out_postion : SV_Position;
    float3 out_color;
};

[shader("vertex")]
vertexOutput vertMain(uint vid : SV_VertexID) {
    vertexOutput output;
    output.out_color = colors[vid];
    output.out_postion = float4(positions[vid], 0.0, 1.0);
    return output;
}

[shader("fragment")]
float3 fragMain(vertexOutput inVert) : SV_Target {
    float3 color = inVert.out_color;
    return color;
}

