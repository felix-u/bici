@vs vertex_shader
out vec2 fragment_texture_coordinate;

void main() {
    vec2 position_ndc;
    vec2 texture_coordinate;

    if (gl_VertexIndex == 0) {
        position_ndc = vec2(-1.0, -1.0);
        texture_coordinate = vec2(0.0, 0.0);
    } else if (gl_VertexIndex == 1) {
        position_ndc = vec2(3.0, -1.0);
        texture_coordinate = vec2(2.0, 0.0);
    } else {
        position_ndc = vec2(-1.0, 3.0);
        texture_coordinate = vec2(0.0, 2.0);
    }

    gl_Position = vec4(position_ndc, 0.0, 1.0);
    fragment_texture_coordinate = texture_coordinate;
}
@end


@fs fragment_shader
layout(binding=0) uniform texture2D input_texture;
layout(binding=0) uniform sampler input_sampler;

in vec2 fragment_texture_coordinate;

out vec4 fragment_color;

void main() {
    // TODO(felix): mod
    fragment_color = texture(sampler2D(input_texture, input_sampler), fragment_texture_coordinate);
}
@end


@program quad vertex_shader fragment_shader
