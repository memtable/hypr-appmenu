#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/helpers/Color.hpp>

#include <systemd/sd-bus.h>
#include <unordered_map>
#include <vector>

struct wf_appmenu_surface;

class CAppMenuManager {
  public:
    CAppMenuManager();
    ~CAppMenuManager();

    void       init();
    void       registerSurface(wf_appmenu_surface* surface, pid_t pid, const char* service_name, const char* object_path);
    void       unregisterSurface(wf_appmenu_surface* surface);

    void       addManagerResource(wl_resource* res);
    void       removeManagerResource(wl_resource* res);

    void       addAppMenuResource(wl_resource* res);
    void       removeAppMenuResource(wl_resource* res);

    wl_global* getGlobal() {
        return m_pGlobal;
    }

    wl_global* m_pGlobal = nullptr;
    sd_bus*    m_pBus    = nullptr;

  private:
    HANDLE                              m_pHandle = nullptr;
    std::unordered_map<pid_t, uint32_t> m_mPidSurfaceCount;
    std::vector<wl_resource*>           m_vManagerResources;
    std::vector<wl_resource*>           m_vAppMenuResources;
};

inline std::unique_ptr<CAppMenuManager> g_pAppMenuManager;
