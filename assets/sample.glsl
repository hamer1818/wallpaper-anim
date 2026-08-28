// GLSL counterpart of assets/sample.hlsl, for the Linux build.
//
// Shadertoy conventions apply: write mainImage() and the renderer supplies
// iResolution, iTime, iTimeDelta, iFrame, iMouse and iDate.

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    // Normalized pixel coordinates (0 to 1)
    vec2 uv = fragCoord / iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0.0, 2.0, 4.0));

    fragColor = vec4(col, 1.0);
}
