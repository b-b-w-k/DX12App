#include"BoxGameApp.h"


int WINAPI wWinMain(
	_In_ HINSTANCE hInstance, 
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
    //DPI感知,让屏幕画面缩放不受计算机显示缩放比例影响
    typedef BOOL(WINAPI* SetProcessDPIAwarenessContext_t)(DPI_AWARENESS_CONTEXT);
    HMODULE hUser32 = LoadLibraryW(L"user32.dll");
    if (hUser32) {
        auto pSetProcessDPIAwarenessContext = (SetProcessDPIAwarenessContext_t)GetProcAddress(hUser32, "SetProcessDPIAwarenessContext");
        if (pSetProcessDPIAwarenessContext) {
            pSetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
        else {
            // 回退到旧版 API
            SetProcessDPIAware();
        }
        FreeLibrary(hUser32);
    }
    else {
        SetProcessDPIAware();
    }


    BoxGameApp app(hInstance);
    if (!app.InitApp())return 0;

    return app.GameRun();


}

