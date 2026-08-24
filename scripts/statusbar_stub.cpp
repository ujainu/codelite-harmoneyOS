// wxStatusBar boot-safe ARM64 stubs for in-place lib patch.
#include <cstdint>

extern "C" __attribute__((visibility("default")))
void __attribute__((naked)) _ZN11wxStatusBar11InitColoursEv()
{
    __asm__("ret\n");
}

extern "C" __attribute__((visibility("default")))
void __attribute__((naked)) _ZN11wxStatusBar7OnPaintER12wxPaintEvent()
{
    __asm__("ret\n");
}

extern "C" __attribute__((visibility("default")))
void __attribute__((naked)) _ZN11wxStatusBar12SetMinHeightEi()
{
    __asm__(
        "cmp w1, #0\n"
        "mov w8, #22\n"
        "csel w8, w1, w8, gt\n"
        "str w8, [x0, #596]\n"
        "ret\n"
    );
}

extern "C" __attribute__((visibility("default")))
void __attribute__((naked)) _ZNK11wxStatusBar19DoGetBestClientSizeEv()
{
    __asm__(
        "mov w8, #100\n"
        "mov w19, #22\n"
        "bfi x19, x8, #32, #32\n"
        "mov x0, x19\n"
        "ret\n"
    );
}

extern "C" __attribute__((visibility("default")))
void __attribute__((naked)) _ZNK11wxStatusBar12GetFieldRectEiR6wxRect()
{
    __asm__(
        "cmp w1, #0\n"
        "b.lt 1f\n"
        "mov w8, #100\n"
        "mov w9, #20\n"
        "str wzr, [x2]\n"
        "str wzr, [x2, #8]\n"
        "str w8, [x2, #4]\n"
        "str w9, [x2, #12]\n"
        "mov w0, #1\n"
        "ret\n"
        "1:\n"
        "mov w0, #0\n"
        "ret\n"
    );
}
