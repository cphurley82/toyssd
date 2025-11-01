// Copyright toyssd contributors
// Lightweight portability tweaks for analysis tools and mixed toolchains
#pragma once

// Some Apple SDK/system headers and third-party code may rely on __has_feature
// in preprocessor conditionals. When clang-tidy parses translation units using
// a different toolchain configuration than the compiler, it can observe cases
// where __has_feature is not defined as a function-like macro. Provide a safe
// fallback so parsing succeeds.
#ifndef __has_feature
#define __has_feature(x) 0
#endif
