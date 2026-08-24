// Boot-safe ARM64 stubs for wxStaticText sizing (host build → byte patch template).
#include <cstdint>

extern "C" __attribute__((visibility("default")))
void __attribute__((naked)) _ZNK19wxGenericStaticText19DoGetBestClientSizeEv()
{
    __asm__(
        "mov w8, #128\n"
        "mov w19, #16\n"
        "bfi x19, x8, #32, #32\n"
        "mov x0, x19\n"
        "ret\n"
    );
}

extern "C" __attribute__((visibility("default")))
void __attribute__((naked)) _ZNK16wxMarkupTextBase7MeasureER12wxReadOnlyDCPi()
{
    __asm__(
        "mov w8, #128\n"
        "mov w9, #16\n"
        "bfi x8, x9, #32, #32\n"
        "mov x0, x8\n"
        "cbz x2, 1f\n"
        "str w9, [x2]\n"
        "1:\n"
        "ret\n"
    );
}
