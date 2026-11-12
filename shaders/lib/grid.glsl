
// thanks to https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8
float grid(float lineWidth, vec2 texCoord) {
    vec2 uvDeriv = fwidth(texCoord * gridTiling);
    bool invertLine = lineWidth > 0.5;
    vec2 targetWidth = vec2(invertLine ? 1.0 - lineWidth : lineWidth);
    vec2 drawWidth = clamp(targetWidth, uvDeriv, vec2(0.5));
    vec2 lineAA = uvDeriv * 1.5;
    vec2 gridUV = abs(fract(texCoord * gridTiling) * 2.0 - 1.0);
    gridUV = invertLine ? gridUV : 1.0 - gridUV;
    vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0.0, 1.0);
    grid2 = mix(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0.0, 1.0));
    grid2 = invertLine ? 1.0 - grid2 : grid2;
    float grid = mix(grid2.x, 1.0, grid2.y);
    return grid;
}
