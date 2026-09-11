#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <iostream>
#include <string>

// dx11 libraries
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

// imgui includes
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"


// define wndproc for ImGui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// offsets
uintptr_t swapchain_offset = 0x2C2C48; // offset of the swapchain in the target process
constexpr int vtable_index_present = 8; // index of the Present in the vtable

// define the function pointer types for Present and ResizeBuffers
typedef HRESULT(__stdcall* present_t)(IDXGISwapChain*, UINT, UINT);

// original wndproc pointer so we can call it after imgui is done processing messages
WNDPROC original_wnd_proc = nullptr; // Stores the original WndProc

// struct to pass through the variables we need for imgui and the hook
struct variables_s {
    // for imgui
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;

    // define the function pointer for present
    present_t original_present = nullptr;
	present_t current_present = nullptr;

	// swap chain pointer
	IDXGISwapChain* pSwapChain = nullptr;

	uintptr_t base = 0; // base address of the target process

}variables;

// include for menu drawing - do it here after the struct so we can pass the variables struct to it
#include "menu.hpp"

// log function for debugging
void log_better(const char* msg, bool success)
{
	// if success, print [+] before the message, else print [-] using snprintf for hexadecimal representation of the message
    	char buffer[256];
    	snprintf(buffer, sizeof(buffer), "%s", msg);
    	std::cout << (success ? "[+] " : "[-] ") << buffer << std::endl;
}

// hooked wndproc for imgui
LRESULT __stdcall hk_wndproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    return CallWindowProcA(original_wnd_proc, hWnd, uMsg, wParam, lParam);
}

// hooked Present
HRESULT __stdcall hk_present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    static bool once = false;
    if (!once) {
        once = true;
        log_better("hkPresent fired!", true);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&variables.pDevice);
        variables.pDevice->GetImmediateContext(&variables.pContext);

        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        HWND hWnd = desc.OutputWindow;
        ImGui_ImplWin32_Init(hWnd);

		variables.pSwapChain = pSwapChain;

        original_wnd_proc = (WNDPROC)SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)hk_wndproc);

        ImGui_ImplDX11_Init(variables.pDevice, variables.pContext);
        io.Fonts->Build();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    draw_menu(variables);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// call the original Present function
    return variables.original_present(pSwapChain, SyncInterval, Flags);
}

// patch the vtable of the swap chain to point to our hooks
bool install_present_hook(IDXGISwapChain* pSwapChain)
{
    if (!pSwapChain) return false;

    void** vtable = *reinterpret_cast<void***>(pSwapChain);

    present_t* present_temp = reinterpret_cast<present_t*>(&vtable[vtable_index_present]);   // 0x40 

	// check if the hook is already installed (not really necessary)
    if (*present_temp == &hk_present) return true;

	// store the original function pointer
    variables.original_present = *present_temp;

	// print out the original function pointer for debugging

    log_better(("Original Present: " + std::to_string((uintptr_t)variables.original_present)).c_str(), true);


    DWORD old_protect;

	// patch the vtable to point to our hook
    VirtualProtect(present_temp, sizeof(present_t), PAGE_EXECUTE_READWRITE, &old_protect);
    *present_temp = &hk_present;
    VirtualProtect(present_temp, sizeof(present_t), old_protect, &old_protect);

	// store the current present function pointer (which is now our hook) for display in the menu
	variables.current_present = *present_temp;

    return true;
}

// worker thread to poll for the swap chain and install the hook
DWORD WINAPI main_thread(LPVOID)
{
	// allocate a console for debugging
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

	// get the base address of the target process and calculate the address of the swap chain pointer
    variables.base = (uintptr_t)GetModuleHandleA(nullptr);
    IDXGISwapChain** ppSwapChain = (IDXGISwapChain**)(variables.base + swapchain_offset);

    log_better(("module base: " + std::to_string(variables.base)).c_str(), true);
    log_better(("Watching @: " + std::to_string((uintptr_t)ppSwapChain)).c_str(), true);

    for (int i = 0; i < 600; ++i) {
        IDXGISwapChain* sc = *ppSwapChain;
        if (sc) {
            log_better(("SwapChain found: " + std::to_string((uintptr_t)sc)).c_str(), true);
            if (install_present_hook(sc))
                log_better("Present hooked.", true);
            else
                log_better("InstallPresentHook failed.", false);
            return 0;
        }
        Sleep(100);
    }
    log_better("Timed out waiting for swap chain.", false);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(0, 0, main_thread, 0, 0, 0);
    }
    return TRUE;
}