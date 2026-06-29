cbuffer ShadertoyGlobals : register(b0) {
    float3 iResolution;
    float iTime;
    float4 iMouse;
}

struct PS_INPUT { 
    float4 pos : SV_POSITION; 
    float2 tex : TEXCOORD; 
};

float4 main(PS_INPUT input) : SV_TARGET {
    // Normalized pixel coordinates (from 0 to 1)
    float2 uv = input.tex;
    
    // Time varying pixel color
    float3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + float3(0, 2, 4));
    
    // Output to screen
    return float4(col, 1.0);
}
