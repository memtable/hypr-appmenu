#include "appMenuPlugin.hpp"
#include "appmenu-protocol.h"

#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <algorithm>

#define APICALL extern "C"
#define EXPORT  __attribute__((visibility("default")))

inline HANDLE PHANDLE = nullptr;

struct wf_appmenu_surface {
    wl_resource* resource                 = nullptr;
    wl_resource* wl_surface               = nullptr;
    wl_listener  surface_destroy_listener = {};
    pid_t        pid                      = 0;
    std::string  service_name;
    std::string  object_path;
    bool         registered               = false;
};

static int onBusEvent(int fd, uint32_t mask, void* data) {
    auto* mgr = static_cast<CAppMenuManager*>(data);
    if (mgr && mgr->m_pBus) {
        while (sd_bus_process(mgr->m_pBus, nullptr) > 0) {}
        mgr->updateBusEventSource();
    }
    return 0;
}

static int onNameOwnerChanged(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
    const char* name      = nullptr;
    const char* old_owner = nullptr;
    const char* new_owner = nullptr;
    int         r         = sd_bus_message_read(m, "sss", &name, &old_owner, &new_owner);
    if (r >= 0 && name && strcmp(name, "com.canonical.AppMenu.Registrar") == 0) {
        if (new_owner && strlen(new_owner) > 0) {
            Log::logger->log(Log::DEBUG, "APPMENU: com.canonical.AppMenu.Registrar appeared (owner: {}), re-registering surfaces", new_owner);
            auto* mgr = static_cast<CAppMenuManager*>(userdata);
            if (mgr)
                mgr->reRegisterAllSurfaces();
        }
    }
    return 0;
}

bool CAppMenuManager::ensureBus() {
    if (m_pBus) {
        if (sd_bus_is_open(m_pBus) > 0)
            return true;
        cleanupBus();
    }

    int r = sd_bus_open_user(&m_pBus);
    if (r < 0) {
        Log::logger->log(Log::ERR, "APPMENU: failed to open user dbus connection: {}", strerror(-r));
        m_pBus = nullptr;
        return false;
    }

    initBus();
    return true;
}

void CAppMenuManager::initBus() {
    if (!m_pBus)
        return;

    int r = sd_bus_match_signal(m_pBus, &m_pBusSlot, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged",
                                onNameOwnerChanged, this);
    if (r < 0) {
        Log::logger->log(Log::WARN, "APPMENU: failed to add NameOwnerChanged match: {}", strerror(-r));
    }

    int fd = sd_bus_get_fd(m_pBus);
    if (fd >= 0 && g_pCompositor && g_pCompositor->m_wlDisplay) {
        wl_event_loop* loop = wl_display_get_event_loop(g_pCompositor->m_wlDisplay);
        if (loop) {
            int      events    = sd_bus_get_events(m_pBus);
            uint32_t wl_events = 0;
            if (events & POLLIN)
                wl_events |= WL_EVENT_READABLE;
            if (events & POLLOUT)
                wl_events |= WL_EVENT_WRITABLE;
            if (events & POLLERR)
                wl_events |= WL_EVENT_ERROR;
            if (events & POLLHUP)
                wl_events |= WL_EVENT_HANGUP;

            m_pBusSource = wl_event_loop_add_fd(loop, fd, wl_events, onBusEvent, this);
        }
    }
}

void CAppMenuManager::cleanupBus() {
    if (m_pBusSlot) {
        sd_bus_slot_unref(m_pBusSlot);
        m_pBusSlot = nullptr;
    }
    if (m_pBusSource) {
        wl_event_source_remove(m_pBusSource);
        m_pBusSource = nullptr;
    }
    if (m_pBus) {
        sd_bus_flush_close_unref(m_pBus);
        m_pBus = nullptr;
    }
}

void CAppMenuManager::updateBusEventSource() {
    if (!m_pBus || !m_pBusSource)
        return;
    int      events    = sd_bus_get_events(m_pBus);
    uint32_t wl_events = 0;
    if (events & POLLIN)
        wl_events |= WL_EVENT_READABLE;
    if (events & POLLOUT)
        wl_events |= WL_EVENT_WRITABLE;
    if (events & POLLERR)
        wl_events |= WL_EVENT_ERROR;
    if (events & POLLHUP)
        wl_events |= WL_EVENT_HANGUP;
    wl_event_source_fd_update(m_pBusSource, wl_events);
}

void CAppMenuManager::sendRegisterSurface(pid_t pid, const char* service_name, const char* object_path) {
    if (!ensureBus() || pid <= 0 || !service_name || !object_path)
        return;

    sd_bus_message* m = nullptr;
    int             r =
        sd_bus_message_new_method_call(m_pBus, &m, "com.canonical.AppMenu.Registrar", "/com/canonical/AppMenu/Registrar", "com.canonical.AppMenu.Registrar", "RegisterSurfaceMenu");
    if (r >= 0) {
        sd_bus_message_append(m, "uss", (uint32_t)pid, service_name, object_path);
        sd_bus_send(m_pBus, m, nullptr);
        sd_bus_flush(m_pBus);
        sd_bus_message_unref(m);
        updateBusEventSource();
    }
}

void CAppMenuManager::sendUnregisterWindow(pid_t pid) {
    if (!ensureBus() || pid <= 0)
        return;

    sd_bus_message* m = nullptr;
    int             r = sd_bus_message_new_method_call(m_pBus, &m, "com.canonical.AppMenu.Registrar", "/com/canonical/AppMenu/Registrar", "com.canonical.AppMenu.Registrar",
                                                       "UnregisterWindow");
    if (r >= 0) {
        sd_bus_message_append(m, "u", (uint32_t)pid);
        sd_bus_send(m_pBus, m, nullptr);
        sd_bus_flush(m_pBus);
        sd_bus_message_unref(m);
        updateBusEventSource();
    }
}

void CAppMenuManager::registerSurface(wf_appmenu_surface* surface, pid_t pid, const char* service_name, const char* object_path) {
    if (!surface || pid <= 0 || !service_name || !object_path)
        return;

    surface->pid          = pid;
    surface->service_name = service_name;
    surface->object_path  = object_path;

    if (!surface->registered) {
        surface->registered = true;
        m_mPidSurfaceCount[pid]++;
    }

    sendRegisterSurface(pid, service_name, object_path);
}

void CAppMenuManager::unregisterSurface(wf_appmenu_surface* surface) {
    if (!surface)
        return;

    removeSurface(surface);

    if (!surface->registered)
        return;

    pid_t pid           = surface->pid;
    surface->registered = false;

    auto it = m_mPidSurfaceCount.find(pid);
    if (it != m_mPidSurfaceCount.end()) {
        it->second--;
        if (it->second == 0) {
            m_mPidSurfaceCount.erase(it);
            sendUnregisterWindow(pid);
        }
    }
}

void CAppMenuManager::reRegisterAllSurfaces() {
    if (!ensureBus())
        return;

    std::unordered_set<pid_t> sentPids;
    for (auto* surface : m_vSurfaces) {
        if (surface && surface->registered && surface->pid > 0 && !surface->service_name.empty() && !surface->object_path.empty()) {
            if (sentPids.insert(surface->pid).second) {
                sendRegisterSurface(surface->pid, surface->service_name.c_str(), surface->object_path.c_str());
            }
        }
    }
}

void CAppMenuManager::addSurface(wf_appmenu_surface* surface) {
    if (surface && std::find(m_vSurfaces.begin(), m_vSurfaces.end(), surface) == m_vSurfaces.end())
        m_vSurfaces.push_back(surface);
}

void CAppMenuManager::removeSurface(wf_appmenu_surface* surface) {
    std::erase(m_vSurfaces, surface);
}

void CAppMenuManager::addManagerResource(wl_resource* res) {
    m_vManagerResources.push_back(res);
}

void CAppMenuManager::removeManagerResource(wl_resource* res) {
    std::erase(m_vManagerResources, res);
}

void CAppMenuManager::addAppMenuResource(wl_resource* res) {
    m_vAppMenuResources.push_back(res);
}

void CAppMenuManager::removeAppMenuResource(wl_resource* res) {
    std::erase(m_vAppMenuResources, res);
}

static void handle_surface_destroy(wl_listener* listener, void* data) {
    wf_appmenu_surface* surface = wl_container_of(listener, surface, surface_destroy_listener);
    surface->wl_surface         = nullptr;
    wl_list_remove(&surface->surface_destroy_listener.link);
    wl_list_init(&surface->surface_destroy_listener.link);

    if (surface->resource) {
        wl_resource_destroy(surface->resource);
    }
}

static void handle_appmenu_destroy(wl_resource* resource) {
    auto* surface = static_cast<wf_appmenu_surface*>(wl_resource_get_user_data(resource));
    if (surface) {
        surface->resource = nullptr;

        if (surface->wl_surface) {
            wl_list_remove(&surface->surface_destroy_listener.link);
            wl_list_init(&surface->surface_destroy_listener.link);
            surface->wl_surface = nullptr;
        }

        if (g_pAppMenuManager) {
            g_pAppMenuManager->unregisterSurface(surface);
            g_pAppMenuManager->removeAppMenuResource(resource);
        }

        delete surface;
    }
}

static void handle_appmenu_set_address(wl_client* client, wl_resource* resource, const char* service_name, const char* object_path) {
    auto* surface = static_cast<wf_appmenu_surface*>(wl_resource_get_user_data(resource));
    if (!surface)
        return;

    pid_t pid = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    wl_client_get_credentials(client, &pid, &uid, &gid);

    Log::logger->log(Log::DEBUG, "APPMENU: set_address from pid {} with service: {} path: {}", pid, service_name, object_path);

    if (g_pAppMenuManager)
        g_pAppMenuManager->registerSurface(surface, pid, service_name, object_path);
}

static void handle_appmenu_release(wl_client* client, wl_resource* resource) {
    wl_resource_destroy(resource);
}

static const struct org_kde_kwin_appmenu_interface appmenu_impl = {
    .set_address = handle_appmenu_set_address,
    .release     = handle_appmenu_release,
};

static void handle_appmenu_manager_create(wl_client* client, wl_resource* resource, uint32_t id, wl_resource* surface) {
    auto* appmenu_surface       = new wf_appmenu_surface;
    appmenu_surface->wl_surface = surface;
    appmenu_surface->resource   = wl_resource_create(client, &org_kde_kwin_appmenu_interface, wl_resource_get_version(resource), id);

    if (!appmenu_surface->resource) {
        Log::logger->log(Log::ERR, "APPMENU: failed to create appmenu resource");
        wl_client_post_no_memory(client);
        delete appmenu_surface;
        return;
    }

    if (surface) {
        appmenu_surface->surface_destroy_listener.notify = handle_surface_destroy;
        wl_resource_add_destroy_listener(surface, &appmenu_surface->surface_destroy_listener);
    } else {
        wl_list_init(&appmenu_surface->surface_destroy_listener.link);
    }

    wl_resource_set_implementation(appmenu_surface->resource, &appmenu_impl, appmenu_surface, handle_appmenu_destroy);

    if (g_pAppMenuManager) {
        g_pAppMenuManager->addAppMenuResource(appmenu_surface->resource);
        g_pAppMenuManager->addSurface(appmenu_surface);
    }
}

static void handle_appmenu_manager_release(wl_client* client, wl_resource* resource) {
    wl_resource_destroy(resource);
}

static const struct org_kde_kwin_appmenu_manager_interface appmenu_manager_impl = {
    .create  = handle_appmenu_manager_create,
    .release = handle_appmenu_manager_release,
};

static void handle_manager_destroy(wl_resource* resource) {
    if (g_pAppMenuManager)
        g_pAppMenuManager->removeManagerResource(resource);
}

static void bind_appmenu(wl_client* client, void* data, uint32_t version, uint32_t id) {
    auto* resource = wl_resource_create(client, &org_kde_kwin_appmenu_manager_interface, version, id);

    if (!resource) {
        Log::logger->log(Log::ERR, "APPMENU: failed to create manager resource");
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &appmenu_manager_impl, nullptr, handle_manager_destroy);

    if (g_pAppMenuManager)
        g_pAppMenuManager->addManagerResource(resource);
}

CAppMenuManager::CAppMenuManager() {}

CAppMenuManager::~CAppMenuManager() {
    if (m_pGlobal) {
        wl_global_destroy(m_pGlobal);
        m_pGlobal = nullptr;
    }

    auto appMenuResources = m_vAppMenuResources;
    for (auto* res : appMenuResources) {
        wl_resource_destroy(res);
    }
    m_vAppMenuResources.clear();

    auto managerResources = m_vManagerResources;
    for (auto* res : managerResources) {
        wl_resource_destroy(res);
    }
    m_vManagerResources.clear();

    cleanupBus();
}

void CAppMenuManager::init() {
    Log::logger->log(Log::DEBUG, "APPMENU: initializing manager");

    ensureBus();

    m_pGlobal = wl_global_create(g_pCompositor->m_wlDisplay, &org_kde_kwin_appmenu_manager_interface, 2, nullptr, bind_appmenu);

    if (!m_pGlobal) {
        Log::logger->log(Log::ERR, "APPMENU: failed to create global!");
        return;
    }

    Log::logger->log(Log::DEBUG, "APPMENU: manager initialized successfully");
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hypr-appmenu] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hypr-appmenu] Version mismatch");
    }

    g_pAppMenuManager = std::make_unique<CAppMenuManager>();
    g_pAppMenuManager->init();

    return {"hypr-appmenu", "AppMenu Protocol Plugin for Hyprland", "memtable", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    Log::logger->log(Log::DEBUG, "APPMENU: plugin shutting down");
    g_pAppMenuManager.reset();
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}
