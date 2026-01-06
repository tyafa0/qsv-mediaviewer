#ifndef SDL_HEADERS_H
#define SDL_HEADERS_H

// MinGW環境におけるQtとSDLの競合を解決するための最終手段
#if defined(_WIN32) && defined(__GNUC__)
#define _INTRIN_H_
#include <intrin.h>
#endif

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>

#endif // SDL_HEADERS_H
