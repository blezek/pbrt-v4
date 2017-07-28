
#include "tests/gtest/gtest.h"

#include "pbrt.h"
#include "image.h"
#include "rng.h"
#include "mipmap.h"
#include "half.h"

using namespace pbrt;

// TODO:
// for tga and png i/o: test mono and rgb; make sure mono is smaller
// pixel bounds stuff... (including i/o paths...)
// basic lookups, bilerps, etc
//   also clamp, repeat, etc...
// resize?
// round trip: init, write, read, check
// FlipY()

TEST(Image, Basics) {
    Image y8(PixelFormat::Y8, {4, 8});
    EXPECT_EQ(y8.nChannels(), 1);
    EXPECT_EQ(y8.BytesUsed(), y8.resolution[0] * y8.resolution[1]);

    Image sy8(PixelFormat::SY8, {4, 8});
    EXPECT_EQ(sy8.nChannels(), 1);
    EXPECT_EQ(sy8.BytesUsed(), sy8.resolution[0] * sy8.resolution[1]);

    Image y16(PixelFormat::Y16, {4, 8});
    EXPECT_EQ(y16.nChannels(), 1);
    EXPECT_EQ(y16.BytesUsed(), 2 * y16.resolution[0] * y16.resolution[1]);

    Image y32(PixelFormat::Y32, {4, 8});
    EXPECT_EQ(y32.nChannels(), 1);
    EXPECT_EQ(y32.BytesUsed(), 4 * y32.resolution[0] * y32.resolution[1]);

    Image rgb8(PixelFormat::RGB8, {4, 8});
    EXPECT_EQ(rgb8.nChannels(), 3);
    EXPECT_EQ(rgb8.BytesUsed(), 3 * rgb8.resolution[0] * rgb8.resolution[1]);

    Image srgb8(PixelFormat::SRGB8, {4, 8});
    EXPECT_EQ(srgb8.nChannels(), 3);
    EXPECT_EQ(srgb8.BytesUsed(), 3 * srgb8.resolution[0] * srgb8.resolution[1]);

    Image rgb16(PixelFormat::RGB16, {4, 16});
    EXPECT_EQ(rgb16.nChannels(), 3);
    EXPECT_EQ(rgb16.BytesUsed(),
              2 * 3 * rgb16.resolution[0] * rgb16.resolution[1]);

    Image rgb32(PixelFormat::RGB32, {4, 32});
    EXPECT_EQ(rgb32.nChannels(), 3);
    EXPECT_EQ(rgb32.BytesUsed(),
              4 * 3 * rgb32.resolution[0] * rgb32.resolution[1]);
}

static Float sRGBRoundTrip(Float v) {
    if (v < 0) return 0;
    else if (v > 1) return 1;
    uint8_t encoded = LinearToSRGB8(v);
    return SRGB8ToLinear(encoded);
}

static std::vector<uint8_t> GetInt8Pixels(Point2i res, int nc) {
    std::vector<uint8_t> r;
    for (int y = 0; y < res[1]; ++y)
        for (int x = 0; x < res[0]; ++x)
            for (int c = 0; c < nc; ++c) r.push_back((x * y + c) % 255);
    return r;
}

static std::vector<Float> GetFloatPixels(Point2i res, int nc) {
    std::vector<Float> p;
    for (int y = 0; y < res[1]; ++y)
        for (int x = 0; x < res[0]; ++x)
            for (int c = 0; c < nc; ++c)
                p.push_back(-.25 +
                            2. * (c + 3 * x + 3 * y * res[0]) /
                                (res[0] * res[1]));
    return p;
}

TEST(Image, GetSetY) {
    Point2i res(9, 3);
    std::vector<Float> yPixels = GetFloatPixels(res, 1);

    for (auto format : {PixelFormat::Y8, PixelFormat::SY8, PixelFormat::Y16,
                        PixelFormat::Y32}) {
        Image image(format, res);
        for (int y = 0; y < res[1]; ++y)
            for (int x = 0; x < res[0]; ++x) {
                image.SetChannel({x, y}, 0, yPixels[y * res[0] + x]);
            }
        for (int y = 0; y < res[1]; ++y)
            for (int x = 0; x < res[0]; ++x) {
                Float v = image.GetChannel({x, y}, 0);
                EXPECT_EQ(v, image.GetY({x, y}));
                switch (format) {
                case PixelFormat::Y32:
                    EXPECT_EQ(v, yPixels[y * res[0] + x]);
                    break;
                case PixelFormat::Y16:
                    EXPECT_EQ(
                        v, HalfToFloat(FloatToHalf(yPixels[y * res[0] + x])));
                    break;
                case PixelFormat::Y8: {
                    Float delta =
                        std::abs(v - Clamp(yPixels[y * res[0] + x], 0, 1));
                    EXPECT_LE(delta, 0.501 / 255.);
                    break;
                }
                case PixelFormat::SY8: {
                    EXPECT_FLOAT_EQ(v, sRGBRoundTrip(yPixels[y * res[0] + x]));
                    break;
                }
                default:  // silence compiler warning
                    break;
                }
            }
    }
}

TEST(Image, GetSetRGB) {
    Point2i res(7, 32);
    std::vector<Float> rgbPixels = GetFloatPixels(res, 3);

    for (auto format : {PixelFormat::RGB8, PixelFormat::SRGB8,
                        PixelFormat::RGB16, PixelFormat::RGB32}) {
        Image image(format, res);
        for (int y = 0; y < res[1]; ++y)
            for (int x = 0; x < res[0]; ++x)
                for (int c = 0; c < 3; ++c)
                    image.SetChannel({x, y}, c,
                                     rgbPixels[3 * y * res[0] + 3 * x + c]);

        for (int y = 0; y < res[1]; ++y)
            for (int x = 0; x < res[0]; ++x) {
                Spectrum s = image.GetSpectrum({x, y});
                Float rgb[3];
                s.ToRGB(rgb);

                for (int c = 0; c < 3; ++c) {
                    // This is assuming Spectrum==RGBSpectrum, which is bad.
                    ASSERT_EQ(sizeof(RGBSpectrum), sizeof(Spectrum));

                    EXPECT_EQ(rgb[c], image.GetChannel({x, y}, c));

                    int offset = 3 * y * res[0] + 3 * x + c;
                    switch (format) {
                    case PixelFormat::RGB32:
                        EXPECT_EQ(rgb[c], rgbPixels[offset]);
                        break;
                    case PixelFormat::RGB16:
                        EXPECT_EQ(rgb[c],
                                  HalfToFloat(FloatToHalf(rgbPixels[offset])));
                        break;
                    case PixelFormat::RGB8: {
                        Float delta =
                            std::abs(rgb[c] - Clamp(rgbPixels[offset], 0, 1));
                        EXPECT_LE(delta, 0.501 / 255.);
                        break;
                    }
                    case PixelFormat::SRGB8: {
                        EXPECT_FLOAT_EQ(rgb[c], sRGBRoundTrip(rgbPixels[offset]));
                        break;
                    }
                    default:  // silence compiler warning
                        break;
                    }
                }
            }
    }
}

TEST(Image, PfmIO) {
    Point2i res(16, 49);
    std::vector<Float> rgbPixels = GetFloatPixels(res, 3);

    Image image(rgbPixels, PixelFormat::RGB32, res);
    EXPECT_TRUE(image.Write("test.pfm"));
    Image read;
    EXPECT_TRUE(Image::Read("test.pfm", &read));

    EXPECT_EQ(image.resolution, read.resolution);
    EXPECT_EQ(read.format, PixelFormat::RGB32);

    for (int y = 0; y < res[1]; ++y)
        for (int x = 0; x < res[0]; ++x)
            for (int c = 0; c < 3; ++c)
                EXPECT_EQ(image.GetChannel({x, y}, c),
                          read.GetChannel({x, y}, c));

    EXPECT_EQ(0, remove("test.pfm"));
}

TEST(Image, ExrIO) {
    Point2i res(16, 49);
    std::vector<Float> rgbPixels = GetFloatPixels(res, 3);

    Image image(rgbPixels, PixelFormat::RGB32, res);
    EXPECT_TRUE(image.Write("test.exr"));
    Image read;
    EXPECT_TRUE(Image::Read("test.exr", &read));

    EXPECT_EQ(image.resolution, read.resolution);
    EXPECT_EQ(read.format, PixelFormat::RGB16);

    for (int y = 0; y < res[1]; ++y)
        for (int x = 0; x < res[0]; ++x)
            for (int c = 0; c < 3; ++c)
                EXPECT_EQ(HalfToFloat(FloatToHalf(image.GetChannel({x, y}, c))),
                          read.GetChannel({x, y}, c));
    EXPECT_EQ(0, remove("test.exr"));
}

TEST(Image, TgaRgbIO) {
    Point2i res(11, 48);
    std::vector<Float> rgbPixels = GetFloatPixels(res, 3);

    Image image(rgbPixels, PixelFormat::RGB32, res);
    EXPECT_TRUE(image.Write("test.tga"));
    Image read;
    EXPECT_TRUE(Image::Read("test.tga", &read));

    EXPECT_EQ(image.resolution, read.resolution);
    EXPECT_EQ(read.format, PixelFormat::SRGB8);

    for (int y = 0; y < res[1]; ++y)
        for (int x = 0; x < res[0]; ++x)
            for (int c = 0; c < 3; ++c)
                EXPECT_FLOAT_EQ(sRGBRoundTrip(image.GetChannel({x, y}, c)),
                                read.GetChannel({x, y}, c))
                    << " x " << x << ", y " << y << ", c " << c << ", orig "
                    << rgbPixels[3 * y * res[0] + 3 * x + c];

    EXPECT_EQ(0, remove("test.tga"));
}

TEST(Image, PngRgbIO) {
    Point2i res(11, 50);
    std::vector<Float> rgbPixels = GetFloatPixels(res, 3);

    Image image(rgbPixels, PixelFormat::RGB32, res);
    EXPECT_TRUE(image.Write("test.png"));
    Image read;
    EXPECT_TRUE(Image::Read("test.png", &read));

    EXPECT_EQ(image.resolution, read.resolution);
    EXPECT_EQ(read.format, PixelFormat::SRGB8);

    for (int y = 0; y < res[1]; ++y)
        for (int x = 0; x < res[0]; ++x)
            for (int c = 0; c < 3; ++c)
                EXPECT_FLOAT_EQ(sRGBRoundTrip(image.GetChannel({x, y}, c)),
                                read.GetChannel({x, y}, c))
                    << " x " << x << ", y " << y << ", c " << c << ", orig "
                    << rgbPixels[3 * y * res[0] + 3 * x + c];

    EXPECT_EQ(0, remove("test.png"));
}

TEST(Image, ToSRGB_LUTAccuracy) {
    const int n = 1024 * 1024;
    double sumErr = 0, maxErr = 0;
    RNG rng;
    for (int i = 0; i < n; ++i) {
        Float v = (i + rng.UniformFloat()) / n;
        Float lut = LinearToSRGB(v);
        Float precise = LinearToSRGBFull(v);
        double err = std::abs(lut - precise);
        sumErr += err;
        maxErr = std::max(err, maxErr);
    }
    // These bounds were measured empirically.
    EXPECT_LT(sumErr / n, 6e-6);  // average error
    EXPECT_LT(maxErr, 0.0015);
}

TEST(Image, SRGB8ToLinear) {
    for (int v = 0; v < 255; ++v) {
        float err = std::abs(SRGBToLinear(v / 255.f) - SRGB8ToLinear(v));
        EXPECT_LT(err, 1e-6);
    }
}

// Monotonicity between the individual segments actually isn't enforced
// when we do the piecewise linear fit, but it should happen naturally
// since the derivative of the underlying function doesn't change fit.
TEST(Image, ToSRGB_LUTMonotonic) {
    for (int i = 1; i < LinearToSRGBPiecewiseSize; ++i) {
        // For each break in the function, we'd like to find a pair of floats
        // such that the second uses the next segment after the one used by
        // the first. To deal with fp rounding error, move down a bunch of floats
        // from the computed split point and then step up one float at a time.
        Float v = Float(i) / LinearToSRGBPiecewiseSize;
        int slop = 100;
        v = NextFloatDown(v, slop);
        bool spanned = true;
        for (int j = 0; j < 2 * slop; ++j) {
            EXPECT_LE(LinearToSRGB(v), LinearToSRGB(NextFloatUp(v)));
            spanned |= int(v * LinearToSRGBPiecewiseSize) !=
                int(NextFloatUp(v) * LinearToSRGBPiecewiseSize);
            v = NextFloatUp(v);
        }
        // Make sure we actually did cross segments at some point.
        EXPECT_TRUE(spanned);
    }
}

///////////////////////////////////////////////////////////////////////////

TEST(ImageTexelProvider, Y32) {
    Point2i res(32, 8);

    // Must be a power of 2, so that the base image isn't resampled when
    // generating the MIP levels.
    ASSERT_TRUE(IsPowerOf2(res[0]) && IsPowerOf2(res[1]));
    PixelFormat format = PixelFormat::Y32;
    ASSERT_EQ(1, nChannels(format));

    std::vector<Float> pixels = GetFloatPixels(res, nChannels(format));
    Image image(pixels, format, res);
    ImageTexelProvider provider(image, WrapMode::Clamp,
                                SpectrumType::Reflectance);

    for (Point2i p : Bounds2i({0, 0}, res)) {
        Float pv = provider.TexelFloat(0, p);
        EXPECT_EQ(image.GetY(p), pv);
        EXPECT_EQ(pixels[p.x + p.y * res.x], pv);
    }
}

TEST(ImageTexelProvider, RGB32) {
    Point2i res(2, 4); //16, 32);
    // Must be a power of 2, so that the base image isn't resampled when
    // generating the MIP levels.
    ASSERT_TRUE(IsPowerOf2(res[0]) && IsPowerOf2(res[1]));
    PixelFormat format = PixelFormat::RGB32;
    ASSERT_EQ(3, nChannels(format));

    std::vector<Float> pixels = GetFloatPixels(res, nChannels(format));
    Image image(pixels, format, res);
    ImageTexelProvider provider(image, WrapMode::Clamp,
                                SpectrumType::Reflectance);

    for (Point2i p : Bounds2i({0, 0}, res)) {
        Spectrum is = image.GetSpectrum(p);
        Spectrum ps = provider.TexelSpectrum(0, p);
        EXPECT_EQ(is, ps) << "At pixel " << p << ", image gives : " << is <<
            ", image provider gives " << ps;
        Float rgb[3];
        is.ToRGB(rgb);
        for (int c = 0; c < 3; ++c) {
            EXPECT_EQ(pixels[3 * (p.x + p.y * res.x) + c], rgb[c]);
        }
    }
}

#if 0
TEST(TiledTexelProvider, Y32) {
  Point2i res(32, 8);
  TestFloatProvider<TiledTexelProvider>(res, PixelFormat::Y32);
}
#endif

#if 0
TEST(TiledTexelProvider, RGB32) {
    Point2i res(16, 32);

    ASSERT_TRUE(IsPowerOf2(res[0]) && IsPowerOf2(res[1]));
    PixelFormat format = PixelFormat::RGB32;
    ASSERT_EQ(3, nChannels(format));

    std::vector<Float> pixels = GetFloatPixels(res, nChannels(format));
    Image image(pixels, format, res);
    const char *fn = "tiledprovider.pfm";
    image.Write(fn);
    TiledTexelProvider provider(fn, WrapMode::Clamp, SpectrumType::Reflectance,
                                false);

    for (Point2i p : Bounds2i({0, 0}, res)) {
        Spectrum is = image.GetSpectrum(p);
        Spectrum ps = provider.TexelSpectrum(0, p);
        // FIXME: this doesn't work with the flip above :-p
        // CO    EXPECT_EQ(is, ps) << is << "vs " << ps;
        Float rgb[3];
        ps.ToRGB(rgb);
        for (int c = 0; c < 3; ++c) {
            EXPECT_EQ(pixels[3 * (p.x + p.y * res.x) + c], rgb[c]);
        }
    }

    EXPECT_EQ(0, remove(fn));
}
#endif
