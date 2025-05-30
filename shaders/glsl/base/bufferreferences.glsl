/*
 * Sogang Univ, Graphics Lab, 2024
 *
 */
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require
//layout(buffer_reference, scalar) buffer Vertices { vec4 v[]; };
//layout(buffer_reference, scalar) buffer Indices { uint i[]; };
layout(buffer_reference, scalar) buffer PrimitiveIds { uint id[]; };