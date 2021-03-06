// pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys.
// The pbrt source code is licensed under the Apache License, Version 2.0.
// SPDX: Apache-2.0

#include <pbrt/materials.h>

#include <pbrt/bsdf.h>
#include <pbrt/bssrdf.h>
#include <pbrt/interaction.h>
#include <pbrt/media.h>
#include <pbrt/paramdict.h>
#include <pbrt/textures.h>
#include <pbrt/util/bits.h>
#include <pbrt/util/color.h>
#include <pbrt/util/colorspace.h>
#include <pbrt/util/error.h>
#include <pbrt/util/file.h>
#include <pbrt/util/math.h>
#include <pbrt/util/memory.h>
#include <pbrt/util/print.h>
#include <pbrt/util/spectrum.h>

#include <cmath>
#include <numeric>
#include <string>

namespace pbrt {

// DielectricMaterial Method Definitions
std::string DielectricMaterial::ToString() const {
    return StringPrintf("[ DielectricMaterial displacement: %s uRoughness: %s "
                        "vRoughness: %s etaF: %s "
                        "etaS: %s remapRoughness: %s ]",
                        displacement, uRoughness, vRoughness, etaF, etaS, remapRoughness);
}

DielectricMaterial *DielectricMaterial::Create(
    const TextureParameterDictionary &parameters, const FileLoc *loc, Allocator alloc) {
    FloatTextureHandle etaF = parameters.GetFloatTextureOrNull("eta", alloc);
    SpectrumTextureHandle etaS =
        parameters.GetSpectrumTextureOrNull("eta", SpectrumType::General, alloc);
    if (etaF && etaS) {
        Warning(loc, "Both \"float\" and \"spectrum\" variants of \"eta\" parameter "
                     "were provided. Ignoring the \"float\" one.");
        etaF = nullptr;
    }
    if (!etaF && !etaS)
        etaF = alloc.new_object<FloatConstantTexture>(1.5);

    FloatTextureHandle uRoughness = parameters.GetFloatTextureOrNull("uroughness", alloc);
    FloatTextureHandle vRoughness = parameters.GetFloatTextureOrNull("vroughness", alloc);
    if (!uRoughness)
        uRoughness = parameters.GetFloatTexture("roughness", 0.f, alloc);
    if (!vRoughness)
        vRoughness = parameters.GetFloatTexture("roughness", 0.f, alloc);

    FloatTextureHandle displacement =
        parameters.GetFloatTextureOrNull("displacement", alloc);
    bool remapRoughness = parameters.GetOneBool("remaproughness", true);
    return alloc.new_object<DielectricMaterial>(uRoughness, vRoughness, etaF, etaS,
                                                displacement, remapRoughness);
}

// ThinDielectricMaterial Method Definitions
std::string ThinDielectricMaterial::ToString() const {
    return StringPrintf("[ ThinDielectricMaterial displacement: %s etaF: %s etaS: %s ]",
                        displacement, etaF, etaS);
}

ThinDielectricMaterial *ThinDielectricMaterial::Create(
    const TextureParameterDictionary &parameters, const FileLoc *loc, Allocator alloc) {
    FloatTextureHandle etaF = parameters.GetFloatTextureOrNull("eta", alloc);
    SpectrumTextureHandle etaS =
        parameters.GetSpectrumTextureOrNull("eta", SpectrumType::General, alloc);
    if (etaF && etaS) {
        Warning(loc, "Both \"float\" and \"spectrum\" variants of \"eta\" parameter "
                     "were provided. Ignoring the \"float\" one.");
        etaF = nullptr;
    }
    if (!etaF && !etaS)
        etaF = alloc.new_object<FloatConstantTexture>(1.5);

    FloatTextureHandle displacement =
        parameters.GetFloatTextureOrNull("displacement", alloc);

    return alloc.new_object<ThinDielectricMaterial>(etaF, etaS, displacement);
}

// MixMaterial Method Definitions
std::string MixMaterial::ToString() const {
    return StringPrintf("[ MixMaterial materials: [ %s %s ] amount: %s ]", materials[0],
                        materials[1], amount);
}

MixMaterial *MixMaterial::Create(MaterialHandle materialHandles[2],
                                 const TextureParameterDictionary &parameters,
                                 const FileLoc *loc, Allocator alloc) {
    FloatTextureHandle amount = parameters.GetFloatTexture("amount", 0.5f, alloc);

    return alloc.new_object<MixMaterial>(materialHandles, amount);
}

// HairMaterial Method Definitions
std::string HairMaterial::ToString() const {
    return StringPrintf("[ HairMaterial sigma_a: %s color: %s eumelanin: %s "
                        "pheomelanin: %s eta: %s beta_m: %s beta_n: %s alpha: %s ]",
                        sigma_a, color, eumelanin, pheomelanin, eta, beta_m, beta_n,
                        alpha);
}

HairMaterial *HairMaterial::Create(const TextureParameterDictionary &parameters,
                                   const FileLoc *loc, Allocator alloc) {
    SpectrumTextureHandle sigma_a =
        parameters.GetSpectrumTextureOrNull("sigma_a", SpectrumType::General, alloc);
    SpectrumTextureHandle color =
        parameters.GetSpectrumTextureOrNull("color", SpectrumType::Reflectance, alloc);
    FloatTextureHandle eumelanin = parameters.GetFloatTextureOrNull("eumelanin", alloc);
    FloatTextureHandle pheomelanin =
        parameters.GetFloatTextureOrNull("pheomelanin", alloc);
    if (sigma_a) {
        if (color)
            Warning(loc, R"(Ignoring "color" parameter since "sigma_a" was provided.)");
        if (eumelanin)
            Warning(loc, "Ignoring \"eumelanin\" parameter since \"sigma_a\" was "
                         "provided.");
        if (pheomelanin)
            Warning(loc, "Ignoring \"pheomelanin\" parameter since \"sigma_a\" was "
                         "provided.");
    } else if (color) {
        if (sigma_a)
            Warning(loc, R"(Ignoring "sigma_a" parameter since "color" was provided.)");
        if (eumelanin)
            Warning(loc, "Ignoring \"eumelanin\" parameter since \"color\" was "
                         "provided.");
        if (pheomelanin)
            Warning(loc, "Ignoring \"pheomelanin\" parameter since \"color\" was "
                         "provided.");
    } else if (eumelanin || pheomelanin) {
        if (sigma_a)
            Warning(loc, "Ignoring \"sigma_a\" parameter since "
                         "\"eumelanin\"/\"pheomelanin\" was provided.");
        if (color)
            Warning(loc, "Ignoring \"color\" parameter since "
                         "\"eumelanin\"/\"pheomelanin\" was provided.");
    } else {
        // Default: brown-ish hair.
        sigma_a = alloc.new_object<SpectrumConstantTexture>(
            alloc.new_object<RGBSpectrum>(HairBxDF::SigmaAFromConcentration(1.3, 0.)));
    }

    FloatTextureHandle eta = parameters.GetFloatTexture("eta", 1.55f, alloc);
    FloatTextureHandle beta_m = parameters.GetFloatTexture("beta_m", 0.3f, alloc);
    FloatTextureHandle beta_n = parameters.GetFloatTexture("beta_n", 0.3f, alloc);
    FloatTextureHandle alpha = parameters.GetFloatTexture("alpha", 2.f, alloc);

    return alloc.new_object<HairMaterial>(sigma_a, color, eumelanin, pheomelanin, eta,
                                          beta_m, beta_n, alpha);
}

// DiffuseMaterial Method Definitions
std::string DiffuseMaterial::ToString() const {
    return StringPrintf("[ DiffuseMaterial displacement: %s reflectance: %s sigma: %s ]",
                        displacement, reflectance, sigma);
}

DiffuseMaterial *DiffuseMaterial::Create(const TextureParameterDictionary &parameters,
                                         const FileLoc *loc, Allocator alloc) {
    SpectrumTextureHandle reflectance = parameters.GetSpectrumTexture(
        "reflectance", nullptr, SpectrumType::Reflectance, alloc);
    if (!reflectance)
        reflectance = alloc.new_object<SpectrumConstantTexture>(
            alloc.new_object<ConstantSpectrum>(0.5f));
    FloatTextureHandle sigma = parameters.GetFloatTexture("sigma", 0.f, alloc);
    FloatTextureHandle displacement =
        parameters.GetFloatTextureOrNull("displacement", alloc);
    return alloc.new_object<DiffuseMaterial>(reflectance, sigma, displacement);
}

// ConductorMaterial Method Definitions
std::string ConductorMaterial::ToString() const {
    return StringPrintf(
        "[ ConductorMaterial displacement: %s eta: %s k: %s uRoughness: %s "
        "vRoughness: %s remapRoughness: %s]",
        displacement, eta, k, uRoughness, vRoughness, remapRoughness);
}

ConductorMaterial *ConductorMaterial::Create(const TextureParameterDictionary &parameters,
                                             const FileLoc *loc, Allocator alloc) {
    SpectrumTextureHandle eta = parameters.GetSpectrumTexture(
        "eta", GetNamedSpectrum("metal-Cu-eta"), SpectrumType::General, alloc);
    SpectrumTextureHandle k = parameters.GetSpectrumTexture(
        "k", GetNamedSpectrum("metal-Cu-k"), SpectrumType::General, alloc);

    FloatTextureHandle uRoughness = parameters.GetFloatTextureOrNull("uroughness", alloc);
    FloatTextureHandle vRoughness = parameters.GetFloatTextureOrNull("vroughness", alloc);
    if (!uRoughness)
        uRoughness = parameters.GetFloatTexture("roughness", 0.f, alloc);
    if (!vRoughness)
        vRoughness = parameters.GetFloatTexture("roughness", 0.f, alloc);

    FloatTextureHandle displacement =
        parameters.GetFloatTextureOrNull("displacement", alloc);
    bool remapRoughness = parameters.GetOneBool("remaproughness", true);
    return alloc.new_object<ConductorMaterial>(eta, k, uRoughness, vRoughness,
                                               displacement, remapRoughness);
}

// CoatedDiffuseMaterial Method Definitions
std::string CoatedDiffuseMaterial::ToString() const {
    return StringPrintf(
        "[ CoatedDiffuseMaterial displacement: %s reflectance: %s uRoughness: %s "
        "vRoughness: %s thickness: %s eta: %s remapRoughness: %s ]",
        displacement, reflectance, uRoughness, vRoughness, thickness, eta,
        remapRoughness);
}

CoatedDiffuseMaterial *CoatedDiffuseMaterial::Create(
    const TextureParameterDictionary &parameters, const FileLoc *loc, Allocator alloc) {
    SpectrumTextureHandle reflectance = parameters.GetSpectrumTexture(
        "reflectance", nullptr, SpectrumType::Reflectance, alloc);
    if (!reflectance)
        reflectance = alloc.new_object<SpectrumConstantTexture>(
            alloc.new_object<ConstantSpectrum>(0.5f));

    FloatTextureHandle uRoughness = parameters.GetFloatTextureOrNull("uroughness", alloc);
    FloatTextureHandle vRoughness = parameters.GetFloatTextureOrNull("vroughness", alloc);
    if (!uRoughness)
        uRoughness = parameters.GetFloatTexture("roughness", 0.f, alloc);
    if (!vRoughness)
        vRoughness = parameters.GetFloatTexture("roughness", 0.f, alloc);

    FloatTextureHandle thickness = parameters.GetFloatTexture("thickness", .01, alloc);
    FloatTextureHandle eta = parameters.GetFloatTexture("eta", 1.5, alloc);

    LayeredBxDFConfig config;
    config.maxDepth = parameters.GetOneInt("maxdepth", config.maxDepth);
    config.nSamples = parameters.GetOneInt("nsamples", config.nSamples);
    config.twoSided = parameters.GetOneBool("twosided", config.twoSided);

    FloatTextureHandle displacement =
        parameters.GetFloatTextureOrNull("displacement", alloc);
    bool remapRoughness = parameters.GetOneBool("remaproughness", true);
    return alloc.new_object<CoatedDiffuseMaterial>(reflectance, uRoughness, vRoughness,
                                                   thickness, eta, displacement,
                                                   remapRoughness, config);
}

std::string CoatedConductorMaterial::ToString() const {
    return StringPrintf("[ CoatedConductorMaterial TODO ]");
}

CoatedConductorMaterial *CoatedConductorMaterial::Create(
    const TextureParameterDictionary &parameters, const FileLoc *loc, Allocator alloc) {
    // interface
    FloatTextureHandle interfaceURoughness =
        parameters.GetFloatTextureOrNull("interface.uroughness", alloc);
    FloatTextureHandle interfaceVRoughness =
        parameters.GetFloatTextureOrNull("interface.vroughness", alloc);
    if (!interfaceURoughness)
        interfaceURoughness =
            parameters.GetFloatTexture("interface.roughness", 0.f, alloc);
    if (!interfaceVRoughness)
        interfaceVRoughness =
            parameters.GetFloatTexture("interface.roughness", 0.f, alloc);

    FloatTextureHandle thickness = parameters.GetFloatTexture("thickness", .01, alloc);
    FloatTextureHandle interfaceEta =
        parameters.GetFloatTexture("interface.eta", 1.5, alloc);

    // conductor
    FloatTextureHandle conductorURoughness =
        parameters.GetFloatTextureOrNull("conductor.uroughness", alloc);
    FloatTextureHandle conductorVRoughness =
        parameters.GetFloatTextureOrNull("conductor.vroughness", alloc);
    if (!conductorURoughness)
        conductorURoughness =
            parameters.GetFloatTexture("conductor.roughness", 0.f, alloc);
    if (!conductorVRoughness)
        conductorVRoughness =
            parameters.GetFloatTexture("conductor.roughness", 0.f, alloc);
    SpectrumTextureHandle conductorEta = parameters.GetSpectrumTexture(
        "conductor.eta", GetNamedSpectrum("metal-Cu-eta"), SpectrumType::General, alloc);
    SpectrumTextureHandle k = parameters.GetSpectrumTexture(
        "conductor.k", GetNamedSpectrum("metal-Cu-k"), SpectrumType::General, alloc);

    LayeredBxDFConfig config;
    config.maxDepth = parameters.GetOneInt("maxdepth", config.maxDepth);
    config.nSamples = parameters.GetOneInt("nsamples", config.nSamples);

    FloatTextureHandle displacement =
        parameters.GetFloatTextureOrNull("displacement", alloc);
    bool remapRoughness = parameters.GetOneBool("remaproughness", true);

    return alloc.new_object<CoatedConductorMaterial>(
        interfaceURoughness, interfaceVRoughness, thickness, interfaceEta,
        conductorURoughness, conductorVRoughness, conductorEta, k, displacement,
        remapRoughness, config);
}

// SubsurfaceMaterial Method Definitions
std::string SubsurfaceMaterial::ToString() const {
    return StringPrintf("[ SubsurfaceMaterial displacment: %s scale: %f "
                        "sigma_a: %s sigma_s: %s "
                        "reflectance: %s mfp: %s uRoughness: %s vRoughness: %s "
                        "eta: %f remapRoughness: %s ]",
                        displacement, scale, sigma_a, sigma_s, reflectance, mfp,
                        uRoughness, vRoughness, eta, remapRoughness);
}

SubsurfaceMaterial *SubsurfaceMaterial::Create(
    const TextureParameterDictionary &parameters, const FileLoc *loc, Allocator alloc) {
    SpectrumTextureHandle sigma_a, sigma_s, reflectance, mfp;

    Float g = parameters.GetOneFloat("g", 0.0f);

    // 4, mutually-exclusive, ways to specify the subsurface properties...
    std::string name = parameters.GetOneString("name", "");
    if (!name.empty()) {
        // 1. By name
        SpectrumHandle sig_a, sig_s;
        if (!GetMediumScatteringProperties(name, &sig_a, &sig_s, alloc))
            ErrorExit(loc, "%s: named medium not found.", name);
        if (g != 0)
            Warning(loc, "Non-zero \"g\" ignored with named scattering coefficients.");
        g = 0; /* Enforce g=0 (the database specifies reduced scattering
                  coefficients) */
        sigma_a = alloc.new_object<SpectrumConstantTexture>(sig_a);
        sigma_s = alloc.new_object<SpectrumConstantTexture>(sig_s);
    } else {
        // 2. sigma_a and sigma_s directly specified
        sigma_a =
            parameters.GetSpectrumTextureOrNull("sigma_a", SpectrumType::General, alloc);
        sigma_s =
            parameters.GetSpectrumTextureOrNull("sigma_s", SpectrumType::General, alloc);
        if (sigma_a && !sigma_s)
            ErrorExit(loc, "Provided \"sigma_a\" parameter without \"sigma_s\".");
        if (sigma_s && !sigma_a)
            ErrorExit(loc, "Provided \"sigma_s\" parameter without \"sigma_a\".");

        if (!sigma_a && !sigma_s) {
            // 3. RGB/Spectrum, reflectance
            reflectance = parameters.GetSpectrumTextureOrNull(
                "reflectance", SpectrumType::Reflectance, alloc);
            if (reflectance) {
                SpectrumHandle one = alloc.new_object<ConstantSpectrum>(1.);
                mfp = parameters.GetSpectrumTexture("mfp", one, SpectrumType::General,
                                                    alloc);
            } else {
                // 4. nothing specified -- use defaults
                RGBSpectrum *defaultSigma_a = alloc.new_object<RGBSpectrum>(
                    *RGBColorSpace::sRGB, RGB(.0011f, .0024f, .014f));
                RGBSpectrum *defaultSigma_s = alloc.new_object<RGBSpectrum>(
                    *RGBColorSpace::sRGB, RGB(2.55f, 3.21f, 3.77f));
                sigma_a = alloc.new_object<SpectrumConstantTexture>(defaultSigma_a);
                sigma_s = alloc.new_object<SpectrumConstantTexture>(defaultSigma_s);
            }
        }
    }

    Float scale = parameters.GetOneFloat("scale", 1.f);
    Float eta = parameters.GetOneFloat("eta", 1.33f);

    FloatTextureHandle uRoughness = parameters.GetFloatTextureOrNull("uroughness", alloc);
    FloatTextureHandle vRoughness = parameters.GetFloatTextureOrNull("vroughness", alloc);
    if (!uRoughness)
        uRoughness = parameters.GetFloatTexture("roughness", 0.f, alloc);
    if (!vRoughness)
        vRoughness = parameters.GetFloatTexture("roughness", 0.f, alloc);

    FloatTextureHandle displacement =
        parameters.GetFloatTextureOrNull("displacement", alloc);
    bool remapRoughness = parameters.GetOneBool("remaproughness", true);
    return alloc.new_object<SubsurfaceMaterial>(scale, sigma_a, sigma_s, reflectance, mfp,
                                                g, eta, uRoughness, vRoughness,
                                                displacement, remapRoughness, alloc);
}

// DiffuseTransmissionMaterial Method Definitions
std::string DiffuseTransmissionMaterial::ToString() const {
    return StringPrintf("[ DiffuseTransmissionMaterial displacment: %s reflectance: %s "
                        "transmittance: %s sigma: %s ]",
                        displacement, reflectance, transmittance, sigma);
}

DiffuseTransmissionMaterial *DiffuseTransmissionMaterial::Create(
    const TextureParameterDictionary &parameters, const FileLoc *loc, Allocator alloc) {
    SpectrumTextureHandle reflectance = parameters.GetSpectrumTexture(
        "reflectance", nullptr, SpectrumType::Reflectance, alloc);
    if (!reflectance)
        reflectance = alloc.new_object<SpectrumConstantTexture>(
            alloc.new_object<ConstantSpectrum>(0.25f));

    SpectrumTextureHandle transmittance = parameters.GetSpectrumTexture(
        "transmittance", nullptr, SpectrumType::Reflectance, alloc);
    if (!transmittance)
        transmittance = alloc.new_object<SpectrumConstantTexture>(
            alloc.new_object<ConstantSpectrum>(0.25f));

    FloatTextureHandle displacement =
        parameters.GetFloatTextureOrNull("displacement", alloc);
    bool remapRoughness = parameters.GetOneBool("remaproughness", true);
    FloatTextureHandle sigma = parameters.GetFloatTexture("sigma", 0.f, alloc);
    Float scale = parameters.GetOneFloat("scale", 1.f);
    return alloc.new_object<DiffuseTransmissionMaterial>(reflectance, transmittance,
                                                         sigma, displacement, scale);
}

MeasuredMaterial::MeasuredMaterial(const std::string &filename,
                                   FloatTextureHandle displacement, Allocator alloc)
    : displacement(displacement) {
    brdf = MeasuredBxDF::BRDFDataFromFile(filename, alloc);
}

std::string MeasuredMaterial::ToString() const {
    return StringPrintf("[ MeasuredMaterial displacement: %s ]", displacement);
}

MeasuredMaterial *MeasuredMaterial::Create(const TextureParameterDictionary &parameters,
                                           const FileLoc *loc, Allocator alloc) {
    std::string filename = ResolveFilename(parameters.GetOneString("filename", ""));
    if (filename.empty()) {
        Error("Filename must be provided for MeasuredMaterial");
        return nullptr;
    }
    FloatTextureHandle displacement =
        parameters.GetFloatTextureOrNull("displacement", alloc);
    return alloc.new_object<MeasuredMaterial>(filename, displacement, alloc);
}

std::string MaterialHandle::ToString() const {
    if (ptr() == nullptr)
        return "(nullptr)";

    auto toStr = [](auto ptr) { return ptr->ToString(); };
    return DispatchCPU(toStr);
}

STAT_COUNTER("Scene/Materials", nMaterialsCreated);

MaterialHandle MaterialHandle::Create(
    const std::string &name, const TextureParameterDictionary &parameters,
    /*const */ std::map<std::string, MaterialHandle> &namedMaterials, const FileLoc *loc,
    Allocator alloc) {
    MaterialHandle material;
    if (name.empty() || name == "none")
        return nullptr;
    else if (name == "diffuse")
        material = DiffuseMaterial::Create(parameters, loc, alloc);
    else if (name == "coateddiffuse")
        material = CoatedDiffuseMaterial::Create(parameters, loc, alloc);
    else if (name == "coatedconductor")
        material = CoatedConductorMaterial::Create(parameters, loc, alloc);
    else if (name == "diffusetransmission")
        material = DiffuseTransmissionMaterial::Create(parameters, loc, alloc);
    else if (name == "dielectric")
        material = DielectricMaterial::Create(parameters, loc, alloc);
    else if (name == "thindielectric")
        material = ThinDielectricMaterial::Create(parameters, loc, alloc);
    else if (name == "hair")
        material = HairMaterial::Create(parameters, loc, alloc);
    else if (name == "conductor")
        material = ConductorMaterial::Create(parameters, loc, alloc);
    else if (name == "measured")
        material = MeasuredMaterial::Create(parameters, loc, alloc);
    else if (name == "subsurface")
        material = SubsurfaceMaterial::Create(parameters, loc, alloc);
    else if (name == "mix") {
        std::vector<std::string> materials = parameters.GetStringArray("materials");
        if (materials.size() != 2)
            ErrorExit(
                "Must provide two values for \"string materials\" for mix material.");

        MaterialHandle materialHandles[2];
        for (int i = 0; i < 2; ++i) {
            auto iter = namedMaterials.find(materials[i]);
            if (iter == namedMaterials.end())
                ErrorExit("%s: named material not found.", materials[i]);
            materialHandles[i] = iter->second;
        }
        material = MixMaterial::Create(materialHandles, parameters, loc, alloc);
    } else
        ErrorExit(loc, "%s: material type unknown.", name);

    if (!material)
        ErrorExit(loc, "%s: unable to create material.", name);

    parameters.ReportUnused();
    ++nMaterialsCreated;
    return material;
}

}  // namespace pbrt
