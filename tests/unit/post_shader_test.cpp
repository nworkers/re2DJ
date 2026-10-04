#include <string>
#include <vector>

#include "re2dj/graphics/post_shader_catalog.h"
#include "re2dj/graphics/post_shader_source.h"

#include "temporary_tree.h"
#include "test_support.h"

namespace
{

using re2dj::graphics::BuildPostShaderProgramSource;
using re2dj::graphics::ChoosePostShader;
using re2dj::graphics::FindPostShader;
using re2dj::graphics::ListPostShaders;
using re2dj::graphics::LoadPostShaderText;
using re2dj::graphics::PostShaderEntry;
using re2dj::graphics::PostShaderProgramSource;

constexpr char kMinimalShader[] =
    "#pragma parameter STRENGTH \"Strength\" 0.5 0.0 1.0 0.05\n"
    "#pragma parameter GAIN \"Gain\" 1.0 0.5 2.0\n"
    "#if defined(VERTEX)\n"
    "void main() {}\n"
    "#elif defined(FRAGMENT)\n"
    "void main() {}\n"
    "#endif\n";

// Parameters come out with their range, a missing step reads as 0, and the
// defines lead each stage.
void CheckSourceAssembly(re2dj::test::Context& context)
{
    PostShaderProgramSource source;
    std::string error;
    RE2DJ_CHECK(context, BuildPostShaderProgramSource(kMinimalShader, &source, &error));
    RE2DJ_CHECK_EQ(context, source.parameters.size(), std::size_t{2});
    if (source.parameters.size() == 2)
    {
        RE2DJ_CHECK_EQ(context, source.parameters[0].name, std::string("STRENGTH"));
        RE2DJ_CHECK_EQ(context, source.parameters[0].description, std::string("Strength"));
        RE2DJ_CHECK_EQ(context, source.parameters[0].initial, 0.5f);
        RE2DJ_CHECK_EQ(context, source.parameters[0].value, 0.5f);
        RE2DJ_CHECK_EQ(context, source.parameters[0].maximum, 1.0f);
        RE2DJ_CHECK_EQ(context, source.parameters[0].step, 0.05f);
        RE2DJ_CHECK_EQ(context, source.parameters[1].step, 0.0f);
    }
    RE2DJ_CHECK(context, source.vertex.rfind("#define VERTEX\n#define PARAMETER_UNIFORM\n", 0) == 0);
    RE2DJ_CHECK(context, source.fragment.rfind("#define FRAGMENT\n#define PARAMETER_UNIFORM\n", 0) == 0);

    // GLSL wants #version first, so the defines follow it.
    const std::string versioned = std::string("#version 120\n") + kMinimalShader;
    RE2DJ_CHECK(context, BuildPostShaderProgramSource(versioned, &source, &error));
    RE2DJ_CHECK(context, source.vertex.rfind("#version 120\n#define VERTEX\n", 0) == 0);

    // A repeated parameter is kept once.
    const std::string repeated =
        std::string("#pragma parameter STRENGTH \"Again\" 0.1 0.0 1.0\n") + kMinimalShader;
    RE2DJ_CHECK(context, BuildPostShaderProgramSource(repeated, &source, &error));
    RE2DJ_CHECK_EQ(context, source.parameters.size(), std::size_t{2});
}

void CheckMalformedSources(re2dj::test::Context& context)
{
    PostShaderProgramSource source;
    std::string error;
    RE2DJ_CHECK(context, !BuildPostShaderProgramSource("", &source, &error));
    RE2DJ_CHECK(context, !BuildPostShaderProgramSource("void main() {}\n", &source, &error));
    RE2DJ_CHECK(context, !error.empty());
    const char* malformed[] = {
        "#pragma parameter 1BAD \"Name\" 0 0 1\n",
        "#pragma parameter NAME Unquoted 0 0 1\n",
        "#pragma parameter NAME \"Unclosed 0 0 1\n",
        "#pragma parameter NAME \"Short\" 0 0\n",
        "#pragma parameter NAME \"Backwards\" 0 1 0\n",
    };
    for (const char* line : malformed)
    {
        error.clear();
        RE2DJ_CHECK(context, !BuildPostShaderProgramSource(std::string(line) + kMinimalShader, &source, &error));
        RE2DJ_CHECK(context, !error.empty());
    }
}

// The built-ins exist in a fixed order and parse; a directory adds its .glsl
// files by name and nothing else.
void CheckCatalog(re2dj::test::Context& context)
{
    re2dj::test::TemporaryTree tree;
    tree.WriteText("b_shader.glsl", kMinimalShader);
    tree.WriteText("a_shader.glsl", kMinimalShader);
    tree.WriteText("notes.txt", "not a shader");
    tree.MakeDirectory("nested.glsl");
    const std::vector<PostShaderEntry> entries = ListPostShaders(tree.root());
    RE2DJ_CHECK_EQ(context, entries.size(), std::size_t{4});
    if (entries.size() == 4)
    {
        RE2DJ_CHECK_EQ(context, entries[0].id, std::string("crt"));
        RE2DJ_CHECK(context, entries[0].builtin);
        RE2DJ_CHECK_EQ(context, entries[1].id, std::string("scanline"));
        RE2DJ_CHECK_EQ(context, entries[2].id, std::string("a_shader.glsl"));
        RE2DJ_CHECK(context, !entries[2].builtin);
        RE2DJ_CHECK_EQ(context, entries[3].id, std::string("b_shader.glsl"));
    }
    for (const char* id : {"crt", "scanline", "a_shader.glsl"})
    {
        const PostShaderEntry* entry = FindPostShader(entries, id);
        RE2DJ_CHECK(context, entry != nullptr);
        std::string text;
        std::string error;
        PostShaderProgramSource source;
        RE2DJ_CHECK(context, entry != nullptr && LoadPostShaderText(*entry, &text, &error));
        RE2DJ_CHECK(context, BuildPostShaderProgramSource(text, &source, &error));
        RE2DJ_CHECK(context, !source.parameters.empty());
    }
    RE2DJ_CHECK(context, FindPostShader(entries, "none") == nullptr);
    RE2DJ_CHECK(context, FindPostShader(entries, "notes.txt") == nullptr);

    // A missing directory still lists the built-ins.
    RE2DJ_CHECK_EQ(context, ListPostShaders(tree.root() / "missing").size(), std::size_t{2});
}

// The command line wins, then a non-empty environment value, then none.
void CheckChoice(re2dj::test::Context& context)
{
    const std::string scanline = "scanline";
    const std::string none = "none";
    RE2DJ_CHECK_EQ(context, ChoosePostShader(&scanline, "crt"), std::string("scanline"));
    RE2DJ_CHECK_EQ(context, ChoosePostShader(&none, "crt"), std::string("none"));
    RE2DJ_CHECK_EQ(context, ChoosePostShader(nullptr, "crt"), std::string("crt"));
    RE2DJ_CHECK_EQ(context, ChoosePostShader(nullptr, ""), std::string("none"));
    RE2DJ_CHECK_EQ(context, ChoosePostShader(nullptr, nullptr), std::string("none"));
}

}  // namespace

void RunPostShaderTests(re2dj::test::Context& context)
{
    CheckSourceAssembly(context);
    CheckMalformedSources(context);
    CheckCatalog(context);
    CheckChoice(context);
}
