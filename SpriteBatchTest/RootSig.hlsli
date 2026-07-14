// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkID=615561

#define CustomSpriteStaticRS \
"RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"            DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |" \
"            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"            DENY_HULL_SHADER_ROOT_ACCESS |" \
"            DENY_MESH_SHADER_ROOT_ACCESS )," \
"DescriptorTable ( SRV(t0), visibility = SHADER_VISIBILITY_PIXEL ),"\
"CBV(b0), "\
"DescriptorTable ( SRV(t1), visibility = SHADER_VISIBILITY_PIXEL ),"\
"StaticSampler(s0,"\
"           filter = FILTER_MIN_MAG_MIP_LINEAR,"\
"           addressU = TEXTURE_ADDRESS_CLAMP,"\
"           addressV = TEXTURE_ADDRESS_CLAMP,"\
"           addressW = TEXTURE_ADDRESS_CLAMP,"\
"           visibility = SHADER_VISIBILITY_PIXEL )"

#define CustomSpriteHeapRS \
"RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"            DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |" \
"            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"            DENY_HULL_SHADER_ROOT_ACCESS |" \
"            DENY_MESH_SHADER_ROOT_ACCESS )," \
"DescriptorTable ( SRV(t0), visibility = SHADER_VISIBILITY_PIXEL ),"\
"CBV(b0), "\
"DescriptorTable ( Sampler(s0), visibility = SHADER_VISIBILITY_PIXEL )," \
"DescriptorTable ( SRV(t1), visibility = SHADER_VISIBILITY_PIXEL )"
