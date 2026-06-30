struct VS_INPUT {
    float3 pos : POSITION;
    float2 tex : TEXCOORD;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

PS_INPUT VSMain(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.tex = input.tex;
    return output;
}

Texture2D<float> texLuma : register(t0);
Texture2D<float2> texChroma : register(t1);
SamplerState samLinear : register(s0);

float4 PSMain(PS_INPUT input) : SV_TARGET {
    float y = texLuma.Sample(samLinear, input.tex);
    float2 uv = texChroma.Sample(samLinear, input.tex);
    
    // NV12 to RGB conversion
    y = 1.16438356f * (y - 0.06274510f);
    float u = uv.x - 0.5f;
    float v = uv.y - 0.5f;
    
    float r = y + 1.59602678f * v;
    float g = y - 0.39176229f * u - 0.81296764f * v;
    float b = y + 2.01723214f * u;
    
    return float4(saturate(r), saturate(g), saturate(b), 1.0f);
}
