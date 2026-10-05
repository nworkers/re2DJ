#include <string>

#include "re2dj/graphics/gl_renderer_identity.h"

#include "test_support.h"

namespace
{

using re2dj::graphics::GlRendererIdentity;
using re2dj::graphics::IsSoftwareGlRenderer;
using re2dj::graphics::MakeGlRendererIdentity;

// The software rasterizers are named wherever they appear in the string and
// in any case; GPU names, including a GPU behind WSL's D3D12 driver, are not.
void CheckSoftwareRenderers(re2dj::test::Context& context)
{
    RE2DJ_CHECK(context, IsSoftwareGlRenderer("llvmpipe (LLVM 15.0.7, 256 bits)"));
    RE2DJ_CHECK(context, IsSoftwareGlRenderer("softpipe"));
    RE2DJ_CHECK(context, IsSoftwareGlRenderer("Mesa X11 swrast"));
    RE2DJ_CHECK(context, IsSoftwareGlRenderer("GDI Generic"));
    RE2DJ_CHECK(context, IsSoftwareGlRenderer("D3D12 (Microsoft Basic Render Driver)"));
    RE2DJ_CHECK(context, IsSoftwareGlRenderer("Google SwiftShader"));
    RE2DJ_CHECK(context, IsSoftwareGlRenderer("LLVMPIPE"));

    RE2DJ_CHECK(context, !IsSoftwareGlRenderer("NVIDIA GeForce RTX 4070/PCIe/SSE2"));
    RE2DJ_CHECK(context, !IsSoftwareGlRenderer("Intel(R) Arc(TM) Graphics"));
    RE2DJ_CHECK(context, !IsSoftwareGlRenderer("AMD Radeon RX 7800 XT"));
    RE2DJ_CHECK(context, !IsSoftwareGlRenderer("D3D12 (NVIDIA GeForce RTX 4070)"));
    RE2DJ_CHECK(context, !IsSoftwareGlRenderer(""));
    RE2DJ_CHECK(context, !IsSoftwareGlRenderer("unknown"));
}

// A missing string reads as "unknown", which is not software.
void CheckIdentity(re2dj::test::Context& context)
{
    const GlRendererIdentity gpu =
        MakeGlRendererIdentity("NVIDIA GeForce RTX 4070/PCIe/SSE2", "NVIDIA Corporation", "2.1.0 NVIDIA", "windows");
    RE2DJ_CHECK_EQ(context, gpu.renderer, std::string("NVIDIA GeForce RTX 4070/PCIe/SSE2"));
    RE2DJ_CHECK_EQ(context, gpu.vendor, std::string("NVIDIA Corporation"));
    RE2DJ_CHECK_EQ(context, gpu.version, std::string("2.1.0 NVIDIA"));
    RE2DJ_CHECK_EQ(context, gpu.video_driver, std::string("windows"));
    RE2DJ_CHECK(context, !gpu.software);

    const GlRendererIdentity software = MakeGlRendererIdentity("llvmpipe", "Mesa", "4.5", "x11");
    RE2DJ_CHECK(context, software.software);

    const GlRendererIdentity missing = MakeGlRendererIdentity(nullptr, "", nullptr, nullptr);
    RE2DJ_CHECK_EQ(context, missing.renderer, std::string("unknown"));
    RE2DJ_CHECK_EQ(context, missing.vendor, std::string("unknown"));
    RE2DJ_CHECK_EQ(context, missing.version, std::string("unknown"));
    RE2DJ_CHECK_EQ(context, missing.video_driver, std::string("unknown"));
    RE2DJ_CHECK(context, !missing.software);
}

}  // namespace

void RunGlRendererIdentityTests(re2dj::test::Context& context)
{
    CheckSoftwareRenderers(context);
    CheckIdentity(context);
}
