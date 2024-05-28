#include <cmath>
#include <cstdlib>
#include <fmt/core.h>
#include <thread>
#include <vk-renderer/vk-renderer.h>
#include <include/error_fmt.h>
#include <sys/wait.h>
#include <sched.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wuninitialized"

#include <include/tsimd_sh.H>
#include <include/TimeMeasurement.H>


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <include/stb_image_write.h>

#pragma GCC diagnostic pop

#include "types.h"
#include "approx.h"
#include "rt.h"

int main(i32 argc, char **argv)
{
    u64 width = 256, height = 256;
    if (argc >= 3)
    {
        width = strtoul(argv[1], NULL, 10);
        if (width == 0)
        {
            fmt::println(stderr, "[ {} ]\tInvaid width set! Setting width to 256.", ERROR_FMT("ERROR"));
            width = 256;
        }
        height = strtoul(argv[2], NULL, 10);
        if (height == 0)
        {
            fmt::println(stderr, "[ {} ]\tInvaid height set! Setting height to 256.", ERROR_FMT("ERROR"));
            height = 256;
        }
    }
    u64 new_width = width, new_height = height;
    
    std::vector<char*> args(argc);
    std::memcpy(args.data(), argv, sizeof(*argv) * argc);
    bool grid = false;
    u64 idx = 0;
    u64 filename_idx = -1;
    for (char *s : args)
    {
        if (std::strncmp(s, "--grid", 7) == 0) grid = true;
        if (std::strncmp(s, "-o", 3) == 0) filename_idx = idx+1;
        ++idx;
    }

    std::vector<gaussian_t> _gaussians;
    if (grid)
    {
        constexpr u8 num = 5;
        for (u8 i = 0; i < num; ++i)
            for (u8 j = 0; j < num; ++j)
                _gaussians.push_back(gaussian_t{
                        .albedo{ 1.f - (i * num + j)/(float)(num*num), 0.f, 0.f + (i * num + j)/(float)(num*num), 1.f },
                        .mu{ -1.f + 1.f/num + i * 1.f/(num/2.f), -1.f + 1.f/num + j * 1.f/(num/2.f), 0.f },
                        .sigma = 1.f/(2 * num),
                        .magnitude = 3.f
                        });
    }
    else
    {
        _gaussians = { gaussian_t{ .albedo{ 0.f, 1.f, 0.f, .1f }, .mu{ .3f, .3f, .5f }, .sigma = 0.1f, .magnitude = 2.f }, gaussian_t{ .albedo{ 0.f, 0.f, 1.f, .7f }, .mu{ -.3f, -.3f, 0.f }, .sigma = 0.4f, .magnitude = .7f }, gaussian_t{ .albedo{ 1.f, 0.f, 0.f, 1.f }, .mu{ 0.f, 0.f, 2.f }, .sigma = .75f, .magnitude = 1.f } };
    }
    std::vector<gaussian_t> staging_gaussians = _gaussians;
    gaussians_t gaussians{ .gaussians = _gaussians, .gaussians_broadcast = gaussian_vec_t::from_gaussians(_gaussians) };
    const vec4f_t origin = { 0.f, 0.f, -5.f };
    
    renderer_t renderer;
    float draw_time = 0.f;
    bool use_spline_approx = false;
    bool use_mirror_approx = false;
    bool use_taylor_approx = false;
    bool use_abramowitz_approx = false;
    bool use_fast_exp = false;
    bool use_simd_transmittance = false;
    bool use_simd_pixels = false;

    if (!renderer.init(width, height, "Test")) return EXIT_FAILURE;
    renderer.custom_imgui = [&](){
        ImGui::Begin("Gaussians");
        for (gaussian_t &g : staging_gaussians) g.imgui_controls();
        ImGui::End();
        ImGui::Begin("Debug");
        ImGui::Text("Draw Time: %f ms", draw_time);
        ImGui::Checkbox("erf spline", &use_spline_approx);
        ImGui::Checkbox("erf mirror", &use_mirror_approx);
        ImGui::Checkbox("erf taylor", &use_taylor_approx);
        ImGui::Checkbox("erf abramowitz", &use_abramowitz_approx);
        ImGui::Checkbox("exp fast", &use_fast_exp);
        ImGui::Checkbox("use simd innermost", &use_simd_transmittance);
        ImGui::Checkbox("use simd pixels", &use_simd_pixels);
        ImGui::End();
    };
    bool running = true;
    std::thread render_thread([&](){ renderer.run(running); });

    u32 *image = (u32*)std::calloc(width * height, sizeof(u32));

    float x[width * height];
    float y[width * height];

    for (u64 i = 0; i < height; ++i)
    {
        for (u64 j = 0; j < width; ++j)
        {
            x[i * width + j] = -1.f + j/(width/2.f);
            y[i * width + j] = -1.f + i/(height/2.f);
        }
    }

    u64 step = 1;

    while (running)
    {
        if (new_width != width || new_height != height)
        {
            width = new_width;
            height = new_height;
            image = (u32*)std::realloc(image, width * height * sizeof(u32));
        }
        gaussians.gaussians = staging_gaussians;
        gaussians.gaussians_broadcast.load_gaussians(staging_gaussians);
        if (use_spline_approx) _erf = spline_erf;
        else if (use_mirror_approx) _erf = spline_erf_mirror;
        else if (use_taylor_approx) _erf = taylor_erf;
        else if (use_abramowitz_approx) _erf = abramowitz_stegun_erf;
        else _erf = std::erf;
        if (use_fast_exp) _exp = fast_exp;
        else _exp = std::exp;
        if (use_simd_transmittance) _transmittance = simd_transmittance;
        else { _transmittance = transmittance; }

        auto start_time = std::chrono::system_clock::now();
        vec4f_t pt = { -1.f, -1.f, 0.f };
        for (u64 i = 0; i < width * height; i += step)
        {
            pt.x = x[i];
            pt.y = y[i];
            vec4f_t dir = pt - origin;
            dir.normalize();
            vec4f_t color = l_hat(origin, dir, gaussians);
            u32 A = 0xFF000000; // final alpha channel is always 1
            u32 R = (u32)(color.x * 255);
            u32 G = (u32)(color.y * 255);
            u32 B = (u32)(color.z * 255);
            image[i] = A | R << 16 | G << 8 | B;
            if (!running) goto cleanup;
        }

        auto end_time = std::chrono::system_clock::now();
        draw_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        if (filename_idx != (u64)-1)
            stbi_write_png(argv[filename_idx], width, height, 4, image, width * 4);
        renderer.stage_image(image, width, height);
    }

cleanup:
    render_thread.join();
    free(image);

    return EXIT_SUCCESS;
}
