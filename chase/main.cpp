#include <SDL2/SDL.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <limits>
#include <random>

constexpr double pi = std::acos(-1);

// dot
struct Dot
{
    float x     = 0.0f;
    float y     = 0.0f;
    float x_vel = 0.0f;
    float y_vel = 0.0f;
    float acc   = 1.0f;

    Uint8 c = 0xFF;
    Uint8 r = 0xBF;
    Uint8 b = 0xEF;

    Dot(float _x, float _y, float angle, float _acc)
      : x_vel(std::cos(angle) * 10.0f)
      , y_vel(std::sin(angle) * 10.0f)
      , x(_x)
      , y(_y)
      , acc(_acc)
    {
    }
};

void
update(Dot& dot, int delta)
{
    if (delta == 0)
        return;
    // speed = 0.1f * ((9.81f * (elapsed * elapsed)) / 2);
    // speed = std::max(speed * 0.9f, std::numeric_limits<float>::min());
    const float f = ((float)delta) * 0.005f;
    dot.acc *= 1.0f - f;
    auto sign = delta / std::abs(delta);
    dot.x += sign * dot.x_vel * dot.acc;
    dot.y += sign * dot.y_vel * dot.acc;

    dot.c = std::max(dot.c - 8, 0x01);
    dot.r = std::max(dot.r - 3, 0x01);
    dot.b = std::max(dot.b - 5, 0x01);
}

void
gravity(Dot& dot, int delta)
{
    dot.y += delta * 0.0981f;
}

void
draw(SDL_Renderer& ren, Dot& dot)
{
    SDL_SetRenderDrawColor(&ren, dot.r, dot.c, dot.b, dot.r);
    SDL_RenderDrawPoint(&ren, dot.x, dot.y);
}

// fader
// angle
void
particle_fun(SDL_Window* win, SDL_Renderer* ren, SDL_Texture* bg)
{

    std::random_device rd;        // Will be used to obtain a seed for the random number engine
    std::mt19937       gen(rd()); // Standard mersenne_twister_engine seeded with rd()

    constexpr double                 distr_size      = 3600.0;
    constexpr double                 distr_nominator = distr_size * 2.0 * pi;
    std::uniform_int_distribution<>  distr(1, distr_size);
    std::uniform_real_distribution<> distr_speed(0.0, 1.0);

    constexpr int layers_n       = 20;
    constexpr int steps          = 100;
    constexpr int dots_n         = layers_n * steps;
    constexpr int fade_period_ms = 800;
    constexpr int overlap_ms     = fade_period_ms / layers_n;

    std::array<int, 2 * layers_n> tail;
    tail.fill(0);

    std::array<Uint8, 3 * layers_n> tail_c;
    Uint8                           start_c = 0xFF;
    tail_c[0] = tail_c[1] = tail_c[2] = start_c;
    const Uint8 div                   = 0xFF / layers_n;

    for (auto i = 1; i < layers_n - 1; ++i)
    {
        auto j        = i * 3;
        tail_c[j + 0] = 0xFF - (i * div);
        tail_c[j + 1] = 0xFF - (i * div);
        tail_c[j + 2] = 0xFF - (i * div);
    }
    tail_c[layers_n - 1] = 0x00;

    std::vector<Dot> dots;
    dots.reserve(layers_n * steps);
    for (int s = 0; s < steps; ++s)
        for (int l = 0; l < layers_n; ++l)
            dots.emplace_back(Dot(100, 100, 1, 0.0f));

    SDL_SetRenderDrawColor(ren, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(ren);

    constexpr int dot_span          = 1;
    auto          now               = SDL_GetTicks();
    auto          before            = now;
    auto          elapsed           = now;
    auto          elapsed_since_gen = fade_period_ms + 1;
    auto          frame_ticks       = now;
    int           d                 = 0;

    SDL_Event e;
    bool      isDone = false;
    int       x = 0, y = 0;
    int       layer_idx = 0;
    bool      paused    = false;
    bool      step_next = false;
    while (!isDone)
    {
        constexpr int paused_tick_step_ms = overlap_ms / 2;
        if (!paused)
            now = SDL_GetTicks();

        elapsed = now - before;
        before  = now;

        auto spawn_dots = [&]
        {
            // generate dots
            elapsed_since_gen += elapsed;
            if (elapsed_since_gen > overlap_ms)
            {
                elapsed_since_gen = 0;
                layer_idx         = ++layer_idx % layers_n;
                for (auto dd = layer_idx * steps; dd < (layer_idx * steps) + steps; ++dd)
                {
                    const auto s = distr_speed(gen);
                    dots[dd]     = Dot(x, y, distr_nominator / distr(gen), s);
                }
            }
        };

        while (SDL_PollEvent(&e) != 0)
        {
            // User requests quit
            if ((e.type == SDL_QUIT) || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q))
                isDone = true;

            if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_p)
                    paused = true;

                if (e.key.keysym.sym == SDLK_o)
                    paused = false;
            }

            if (e.type == SDL_KEYUP)
            {
                if (e.key.keysym.sym == SDLK_RIGHT)
                    now += paused_tick_step_ms;
                if (e.key.keysym.sym == SDLK_LEFT)
                    now -= paused_tick_step_ms;
            }

            spawn_dots();
        }

        if (paused)
            spawn_dots();

        // mouse pointer
        for (auto i = layers_n - 1; i > 0; --i)
        {
            tail[2 * i]       = tail[2 * (i - 1)];
            tail[(2 * i) + 1] = tail[(2 * (i - 1)) + 1];
        }
        SDL_GetMouseState(&x, &y);
        tail[0] = x;
        tail[1] = y;

        SDL_SetRenderDrawColor(ren, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderClear(ren);
        for (auto j = layers_n - 1; j > -1; --j)
        {
            for (auto i = -dot_span; i < dot_span; ++i)
            {
                x = tail[j * 2 + 0];
                y = tail[j * 2 + 1];

                SDL_SetRenderDrawColor(ren, tail_c[3 * j + 0], tail_c[3 * j + 1], tail_c[3 * j + 2], 0xFF);

                SDL_RenderDrawPoint(ren, x + i, y - dot_span);
                SDL_RenderDrawPoint(ren, x + i, y + dot_span);
                SDL_RenderDrawPoint(ren, x - dot_span, y + i);
                SDL_RenderDrawPoint(ren, x + dot_span, y + i);
            }
        }

        for (auto& d : dots)
            update(d, elapsed);

        for (auto& d : dots)
            gravity(d, elapsed);

        for (auto& d : dots)
            draw(*ren, d);

        SDL_RenderPresent(ren);
    }
    SDL_Delay(10);
}

int
main()
{
    using std::cout;
    using std::endl;

    constexpr int screenW = 1280;
    constexpr int screenH = 768;

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        cout << "SDL_Init Error: " << SDL_GetError() << endl;
        return EXIT_FAILURE;
    }

    SDL_Window* win = SDL_CreateWindow("Hello World!", 100, 100, screenW, screenH, SDL_WINDOW_SHOWN);
    if (win == nullptr)
    {
        cout << "SDL_CreateWindow Error: " << SDL_GetError() << endl;
        return EXIT_FAILURE;
    }
    SDL_SetWindowFullscreen(win, 1);
    SDL_ShowCursor(0);

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (ren == nullptr)
    {
        cout << "SDL_CreateRenderer Error" << SDL_GetError() << endl;
        return EXIT_FAILURE;
    }

    SDL_Texture* bg = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, screenW, screenH);

    particle_fun(win, ren, bg);
    // texture_example(win, ren, bg);

    SDL_DestroyTexture(bg);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return EXIT_SUCCESS;
}
