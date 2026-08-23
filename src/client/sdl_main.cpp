#include "smac/core/game_state.hpp"
#include "smac/core/pathfinding.hpp"
#include "smac/formats/data_directory.hpp"
#include "smac/formats/rules.hpp"
#include "smac/formats/terran_map.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>
namespace sc = smac::core;
namespace sf = smac::formats;
int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "smac-native " << SDL_MAJOR_VERSION << '.' << SDL_MINOR_VERSION << '.'
                  << SDL_MICRO_VERSION << " (SDL)\n";
        return 0;
    }
    if (argc != 3 || std::string(argv[1]) != "--data-dir") {
        std::cerr << "usage: smac-native --data-dir DIR\n";
        return 2;
    }
    auto report = sf::validate_data_directory(argv[2]);
    if (!report.valid()) {
        std::cerr << "invalid data directory; run smac-tool verify-data\n";
        return 1;
    }
    auto map_path = sf::find_case_insensitive(argv[2], "maps/xplanet.MP");
    auto parsed = sf::load_terran_map(*map_path);
    if (auto* e = std::get_if<sf::Error>(&parsed)) {
        std::cerr << e->message << '\n';
        return 1;
    }
    const auto rules_path = sf::find_case_insensitive(argv[2], "alphax.txt");
    auto parsed_rules = sf::load_rules(*rules_path);
    if (auto* error = std::get_if<sf::Error>(&parsed_rules)) {
        std::cerr << error->message << '\n';
        return 1;
    }
    sc::GameState game(std::get<sf::TerranMap>(parsed).to_world_map(),
                       std::get<sf::ParsedRules>(parsed_rules).database);
    sc::MapPosition spawn{};
    bool found = false;
    for (int y = 1; y < game.map().height() - 1 && !found; ++y)
        for (int x = y & 1; x < game.map().width(); x += 2)
            if (game.map().at({x, y}).terrain == sc::Terrain::land) {
                spawn = {x, y};
                found = true;
                break;
            }
    game.units().push_back(
        sc::make_unit(1, 1, spawn, sc::Chassis::native_life, sc::Domain::land, game.rules()));
    if (!SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Window* w = nullptr;
    SDL_Renderer* r = nullptr;
    if (!SDL_CreateWindowAndRenderer("SMAC Native", 1280, 800,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &w,
                                     &r)) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    auto unit_path = sf::find_case_insensitive(argv[2], "Units.pcx");
    SDL_Texture* unit_texture = IMG_LoadTexture(r, unit_path->string().c_str());
    auto font_path = sf::find_case_insensitive(argv[2], "ALPHC___.TTF");
    TTF_Font* font = font_path ? TTF_OpenFont(font_path->string().c_str(), 18.0F) : nullptr;
    bool running = true, dragging = false;
    float zoom = 1.0F, ox = 40, oy = 40;
    sc::MapPosition selected = spawn;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_RETURN:
                    game.apply(sc::EndTurn{});
                    break;
                case SDLK_LEFT:
                    ox += 24;
                    break;
                case SDLK_RIGHT:
                    ox -= 24;
                    break;
                case SDLK_UP:
                    oy += 18;
                    break;
                case SDLK_DOWN:
                    oy -= 18;
                    break;
                case SDLK_EQUALS:
                    zoom = std::min(2.5F, zoom + .25F);
                    break;
                case SDLK_MINUS:
                    zoom = std::max(.5F, zoom - .25F);
                    break;
                default:
                    break;
                }
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                dragging = true;
                float tw = 32 * zoom, th = 20 * zoom;
                int y = static_cast<int>((e.button.y - oy) / th);
                int x = static_cast<int>(((e.button.x - ox) / (tw / 2)));
                if (((x ^ y) & 1) != 0)
                    ++x;
                if (auto p = game.map().normalize({x, y})) {
                    selected = *p;
                    auto path = sc::find_path(game, game.units()[0].id, *p,
                                              game.units()[0].movement_remaining);
                    for (std::size_t i = 1; i < path.size(); ++i)
                        game.apply(sc::MoveUnit{1, path[i]});
                }
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
                dragging = false;
            if (e.type == SDL_EVENT_MOUSE_MOTION && dragging) {
                ox += e.motion.xrel;
                oy += e.motion.yrel;
            }
            if (e.type == SDL_EVENT_MOUSE_WHEEL)
                zoom = std::clamp(zoom + e.wheel.y * .1F, .5F, 2.5F);
        }
        SDL_SetRenderDrawColor(r, 8, 15, 20, 255);
        SDL_RenderClear(r);
        float tw = 32 * zoom, th = 20 * zoom;
        for (int y = 0; y < game.map().height(); ++y)
            for (int x = y & 1; x < game.map().width(); x += 2) {
                auto& p = game.map().at({x, y});
                float sx = ox + static_cast<float>(x) * (tw / 2),
                      sy = oy + static_cast<float>(y) * th;
                if (p.terrain == sc::Terrain::ocean)
                    SDL_SetRenderDrawColor(r, 18, 62, 92, 255);
                else
                    SDL_SetRenderDrawColor(r, 66 + static_cast<Uint8>(p.contour / 5), 92, 45, 255);
                SDL_FRect rect{sx, sy, tw, th};
                SDL_RenderFillRect(r, &rect);
                if (sc::MapPosition{x, y} == selected) {
                    SDL_SetRenderDrawColor(r, 245, 210, 60, 255);
                    SDL_RenderRect(r, &rect);
                }
            }
        auto& u = game.units()[0];
        SDL_FRect ur{ox + static_cast<float>(u.position.x) * (tw / 2),
                     oy + static_cast<float>(u.position.y) * th, tw, th * 1.5F};
        if (unit_texture) {
            SDL_FRect src{0, 0, 64, 48};
            SDL_RenderTexture(r, unit_texture, &src, &ur);
        } else {
            SDL_SetRenderDrawColor(r, 70, 220, 120, 255);
            SDL_RenderFillRect(r, &ur);
        }
        if (font) {
            auto hud = std::string("Turn ") + std::to_string(game.turn()) + "  Tile " +
                       std::to_string(selected.x) + "," + std::to_string(selected.y) + "  MP " +
                       std::to_string(u.movement_remaining);
            SDL_Color color{220, 225, 190, 255};
            if (auto* s = TTF_RenderText_Blended(font, hud.c_str(), hud.size(), color)) {
                auto* t = SDL_CreateTextureFromSurface(r, s);
                SDL_FRect dst{16, 16, static_cast<float>(s->w), static_cast<float>(s->h)};
                SDL_RenderTexture(r, t, nullptr, &dst);
                SDL_DestroyTexture(t);
                SDL_DestroySurface(s);
            }
        }
        SDL_RenderPresent(r);
    }
    if (font)
        TTF_CloseFont(font);
    SDL_DestroyTexture(unit_texture);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
