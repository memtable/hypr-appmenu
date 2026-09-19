#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/helpers/Color.hpp>

#include <systemd/sd-bus.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

struct wf_appmenu_surface;

class CAppMenuManager {
  public:
    CAppMenuManager();
    ~CAppMenuManager();

    void       init();
    void       registerSurface(wf_appmenu_surface* surface, pid_t pid, const char* service_name, const char* object_path);
    void       unregisterSurface(wf_appmenu_surface* surface);
    void       reRegisterAllSurfaces();

    void       addSurface(wf_appmenu_surface* surface);
    void       removeSurface(wf_appmenu_surface* surface);

    void       addManagerResource(wl_resource* res);
    void       removeManagerResource(wl_resource* res);

    void       addAppMenuResource(wl_resource* res);
    void       removeAppMenuResource(wl_resource* res);

    void       updateBusEventSource();

    wl_global* getGlobal() {
        return m_pGlobal;
    }

    wl_global*       m_pGlobal    = nullptr;
    sd_bus*          m_pBus       = nullptr;
    sd_bus_slot*     m_pBusSlot   = nullptr;
    wl_event_source* m_pBusSource = nullptr;

  private:
    bool                                ensureBus();
    void                                initBus();
    void                                cleanupBus();
    void                                sendRegisterSurface(pid_t pid, const char* service_name, const char* object_path);
    void                                sendUnregisterWindow(pid_t pid);

    HANDLE                              m_pHandle = nullptr;
    std::unordered_map<pid_t, uint32_t> m_mPidSurfaceCount;
    std::vector<wf_appmenu_surface*>    m_vSurfaces;
    std::vector<wl_resource*>           m_vManagerResources;
    std::vector<wl_resource*>           m_vAppMenuResources;
};

inline std::unique_ptr<CAppMenuManager> g_pAppMenuManager;
