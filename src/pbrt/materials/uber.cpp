
/*
    pbrt source code is Copyright(c) 1998-2016
                        Matt Pharr, Greg Humphreys, and Wenzel Jakob.

    This file is part of pbrt.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */


// materials/uber.cpp*
#include <pbrt/materials/uber.h>

#include <pbrt/util/memory.h>
#include <pbrt/core/spectrum.h>
#include <pbrt/core/microfacet.h>
#include <pbrt/core/reflection.h>
#include <pbrt/core/texture.h>
#include <pbrt/core/interaction.h>
#include <pbrt/core/paramset.h>

namespace pbrt {

// UberMaterial Method Definitions
void UberMaterial::ComputeScatteringFunctions(SurfaceInteraction *si,
                                              MemoryArena &arena,
                                              TransportMode mode) const {
    // Perform bump mapping with _bumpMap_, if present
    if (bumpMap) Bump(*bumpMap, si);
    Float e = eta->Evaluate(*si);

    Spectrum op = opacity->Evaluate(*si).Clamp();
    Spectrum t = (-op + Spectrum(1.f)).Clamp();
    if (t) {
        si->bsdf = arena.Alloc<BSDF>(*si, 1.f);
        BxDF *tr = arena.Alloc<SpecularTransmission>(t, 1.f, 1.f, mode);
        si->bsdf->Add(tr);
    } else
        si->bsdf = arena.Alloc<BSDF>(*si, e);

    Spectrum kd = op * Kd->Evaluate(*si).Clamp();
    if (kd) {
        BxDF *diff = arena.Alloc<LambertianReflection>(kd);
        si->bsdf->Add(diff);
    }

    Spectrum ks = op * Ks->Evaluate(*si).Clamp();
    if (ks) {
        Fresnel *fresnel = arena.Alloc<FresnelDielectric>(1.f, e);
        Float roughu, roughv;
        if (roughnessu)
            roughu = roughnessu->Evaluate(*si);
        else
            roughu = roughness->Evaluate(*si);
        if (roughnessv)
            roughv = roughnessv->Evaluate(*si);
        else
            roughv = roughu;
        if (remapRoughness) {
            roughu = TrowbridgeReitzDistribution::RoughnessToAlpha(roughu);
            roughv = TrowbridgeReitzDistribution::RoughnessToAlpha(roughv);
        }
        MicrofacetDistribution *distrib =
            arena.Alloc<TrowbridgeReitzDistribution>(roughu, roughv);
        BxDF *spec =
            arena.Alloc<MicrofacetReflection>(ks, distrib, fresnel);
        si->bsdf->Add(spec);
    }

    Spectrum kr = op * Kr->Evaluate(*si).Clamp();
    if (kr) {
        Fresnel *fresnel = arena.Alloc<FresnelDielectric>(1.f, e);
        si->bsdf->Add(arena.Alloc<SpecularReflection>(kr, fresnel));
    }

    Spectrum kt = op * Kt->Evaluate(*si).Clamp();
    if (kt)
        si->bsdf->Add(arena.Alloc<SpecularTransmission>(kt, 1.f, e, mode));
}

std::shared_ptr<UberMaterial> CreateUberMaterial(
    const TextureParams &mp, const std::shared_ptr<const ParamSet> &attributes) {
    std::shared_ptr<Texture<Spectrum>> Kd =
        mp.GetSpectrumTexture("Kd", Spectrum(0.25f));
    std::shared_ptr<Texture<Spectrum>> Ks =
        mp.GetSpectrumTexture("Ks", Spectrum(0.25f));
    std::shared_ptr<Texture<Spectrum>> Kr =
        mp.GetSpectrumTexture("Kr", Spectrum(0.f));
    std::shared_ptr<Texture<Spectrum>> Kt =
        mp.GetSpectrumTexture("Kt", Spectrum(0.f));
    std::shared_ptr<Texture<Float>> roughness =
        mp.GetFloatTexture("roughness", .1f);
    std::shared_ptr<Texture<Float>> uroughness =
        mp.GetFloatTextureOrNull("uroughness");
    std::shared_ptr<Texture<Float>> vroughness =
        mp.GetFloatTextureOrNull("vroughness");
    std::shared_ptr<Texture<Float>> eta = mp.GetFloatTextureOrNull("eta");
    if (!eta) eta = mp.GetFloatTexture("index", 1.5f);
    std::shared_ptr<Texture<Spectrum>> opacity =
        mp.GetSpectrumTexture("opacity", 1.f);
    std::shared_ptr<Texture<Float>> bumpMap =
        mp.GetFloatTextureOrNull("bumpmap");
    bool remapRoughness = mp.GetOneBool("remaproughness", true);
    return std::make_shared<UberMaterial>(Kd, Ks, Kr, Kt, roughness, uroughness,
                                          vroughness, opacity, eta, bumpMap,
                                          remapRoughness, attributes);
}

}  // namespace pbrt