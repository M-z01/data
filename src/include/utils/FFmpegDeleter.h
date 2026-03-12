// Internal-only header — RAII deleter for FFmpeg objects.
// Wraps any FFmpeg free function of the form void f(T**).
#pragma once

template<typename T, void (*Fn)(T**)>
struct FFmpegDeleter {
    void operator()(T* p) const { if (p) Fn(&p); }
};
