struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

cbuffer cb0 : register(b0)
{
    float angle; // radians
}

// Vertex Shader
PSInput VSMain(unsigned int index : SV_VertexID)
{
    PSInput result;

    static float2 positions[3] =
    {
        float2(-0.43f, -0.25f),
        float2(0.0f, 0.5f),
        float2(0.43f, -0.25f)
    };

    static float3 colors[3] =
    {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    };

    float2 inputPos = positions[index];

    // Compute the rotation matrix
    float cosTheta = cos(angle);
    float sinTheta = sin(angle);

    float2 rotatedPos;
    rotatedPos.x = inputPos.x * cosTheta - inputPos.y * sinTheta;
    rotatedPos.y = inputPos.x * sinTheta + inputPos.y * cosTheta;

    result.position = float4(rotatedPos.x, rotatedPos.y, 0.0f, 1.0f);
    result.color = colors[index];

    return result;
}

// Pixel Shader
float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(input.color.r, input.color.g, input.color.b, 1.0);
}
