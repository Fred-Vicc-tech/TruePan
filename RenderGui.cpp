// RenderGui.cpp - ritar TruePans fönster (fram- och baksida) till PNG-filer med
// exakt samma kod som pluginet. Körs i bygget; bilderna laddas upp separat.
#include "TruePanCore.h"
#include <cstdio>

int main()
{
    truepan::Core core;                       // standardinställningar
    const bool a = truepan::editorRenderPng(&core, false, L"TruePan_front.png");
    const bool b = truepan::editorRenderPng(&core, true, L"TruePan_back.png");
    std::printf("front %s, back %s\n", a ? "ok" : "FAILED", b ? "ok" : "FAILED");
    return (a && b) ? 0 : 1;
}
