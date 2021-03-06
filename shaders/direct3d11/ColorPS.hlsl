// Copyright (C) 2015 Elviss Strazdins
// This file is part of the Ouzel engine.

cbuffer Constants : register(b0)
{
    float4 color;
}

struct VS2PS
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

SamplerState Sampler0;

void main(in VS2PS input, out float4 output0 : SV_TARGET0)
{
    output0 = input.color * color;
}
