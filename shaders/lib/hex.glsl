
vec3 hex3(int hex) {
    return vec3(
        ((hex >> 16) & 0xFF) / 255.0, 
        ((hex >> 8) & 0xFF) / 255.0, 
        ((hex) & 0xFF) / 255.0  
    );
}
vec4 hex4(int hex) {
    return vec4(
        ((hex >> 32) & 0xFF) / 255.0, 
        ((hex >> 16) & 0xFF) / 255.0, 
        ((hex >> 8) & 0xFF) / 255.0, 
        ((hex) & 0xFF) / 255.0  
    );
}