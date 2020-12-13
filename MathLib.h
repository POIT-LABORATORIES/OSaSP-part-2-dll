#pragma once
#define MATLIB_API extern "C" __declspec(dllexport)

MATLIB_API int Sum(int x, int y);

MATLIB_API int Sub(int x, int y);

MATLIB_API int sqr(int x);