#include "hydro_manager.h"
#include "state_manager.h"
#include "storage_manager.h"
#include "time_manager.h"
#include "relays.h"
#include "config.h"
#include <esp_log.h>

static const char *TAG = "HYDRO_MANAGER";

static bool pompa_apa_status = false;
static bool pompa_aer_status = false;

// =============================================
// EVENT HANDLER
// =============================================
static void hydro_state_event_handler(void *arg, esp_event_base_t base,
    int32_t event_id, void *event_data) {
    if (base != SYSTEM_EVENTS) return;

    switch (event_id) {
        case SYS_EVENT_STATE_CHANGED: {
            state_change_event_t *ev = (state_change_event_t *)event_data;

            if (ev->new_state == SYS_STATE_MAINTENANCE) {
                ESP_LOGW(TAG, "MAINTENANCE MODE → Forcing pumps OFF");
                pompa_apa_status = false;
                pompa_aer_status = false;
                switch_relay(RELAY_WATER_PUMP_GPIO, false);
                switch_relay(RELAY_AIR_PUMP_GPIO, false);
                storage_log_printf(LOG_TYPE_SYSTEM, "\n%s - MAINTENANCE: Pumps forced OFF",
                    time_manager_get_formatted_time());
            }
            else if (ev->new_state == SYS_STATE_NORMAL) {
                ESP_LOGI(TAG, "NORMAL MODE → Resuming automatic control");
                storage_log_printf(LOG_TYPE_SYSTEM, "\n%s - NORMAL: Automatic hydro control resumed",
                    time_manager_get_formatted_time());
            }
            break;
        }

        case SYS_EVENT_MAINTENANCE_END:
            ESP_LOGI(TAG, "Maintenance ended → resuming normal operation");
            break;
    }
}

// Funcție care determină dacă acum este "ZI" sau "NOAPTE"
static bool is_day_time(int current_min_of_day, hydro_settings_t settings) {
    int start_min_of_day = settings.start_hour * 60 + settings.start_minute;
    int duration_minutes = settings.day_period_hours * 60;
    int end_min_of_day = start_min_of_day + duration_minutes;

    if (end_min_of_day < 1440) {
        if (current_min_of_day >= start_min_of_day && current_min_of_day < end_min_of_day) {
            return true;
        }
    }
    else {
        int end_min_wrapped = end_min_of_day - 1440;
        if (current_min_of_day >= start_min_of_day || current_min_of_day < end_min_wrapped) {
            return true;
        }
    }
    return false;
}

static int get_current_minute_of_day(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    return (timeinfo.tm_hour * 60) + timeinfo.tm_min;
}

// =============================================
// LOGICĂ PRINCIPALĂ MODIFICATĂ (ZI VS NOAPTE)
// =============================================
static void hydro_control_loop(void) {
    if (state_manager_is_in_maintenance()) {
        return;
    }

        // 🔏 PROTECȚIE FIRST BOOT / UNCONFIGURED SYSTEM (FAIL-SAFE)
    // ========================================================================
    // Dacă timpii de zi SAU cei de noapte sunt 0, oprim pompele și ieșim din funcție
    if ((hydro_settings.water_on_min == 0 && hydro_settings.water_off_min == 0) ||
        (hydro_settings.water_night_on_min == 0 && hydro_settings.water_night_off_min == 0)) {
        // Forțăm starea oprită hardware pe ambele relee pentru siguranță absolută
        if (pompa_apa_status || pompa_aer_status) {
            pompa_apa_status = false;
            pompa_aer_status = false;
            switch_relay(RELAY_WATER_PUMP_GPIO, false);
            switch_relay(RELAY_AIR_PUMP_GPIO, false);

            ESP_LOGW(TAG, "Sistem neconfigurat detectat (timpii sunt 0). Pompele au fost oprite.");
        }
        return; // Oprim execuția buclei pentru a preveni împărțirea la zero!
    }

    int current_min_of_day = get_current_minute_of_day();
    bool este_zi = is_day_time(current_min_of_day, hydro_settings);

    // Safety check: Dacă rezervorul e gol → Oprim tot direct hardware
    if (sys_manager.tank_low_flag) {
        if (pompa_apa_status || pompa_aer_status) {
            pompa_apa_status = false;
            pompa_aer_status = false;
            switch_relay(RELAY_WATER_PUMP_GPIO, false);
            switch_relay(RELAY_AIR_PUMP_GPIO, false);

            storage_log_printf(LOG_TYPE_SYSTEM,
                "\n%s - SAFETY: Tank low → All pumps FORCED OFF",
                time_manager_get_formatted_time());
        }
        return;
    }

    // 1. Calculăm minutele scurse de la începutul fazei curente (pentru o formulă modulo stabilă)
    int start_min = (hydro_settings.start_hour * 60) + hydro_settings.start_minute;
    int minute_in_phase = 0;

    if (este_zi) {
        minute_in_phase = (current_min_of_day - start_min + 1440) % 1440;
    }
    else {
     // Noaptea începe exact când se termină ziua
        int night_start_min = (start_min + (hydro_settings.day_period_hours * 60)) % 1440;
        minute_in_phase = (current_min_of_day - night_start_min + 1440) % 1440;
    }

    // 2. Selectăm parametrii de timp corecți în funcție de starea ZI/NOAPTE
    int total_ciclu = 0;
    int timp_on = 0;

    if (este_zi) {
        total_ciclu = hydro_settings.water_on_min + hydro_settings.water_off_min;
        timp_on = hydro_settings.water_on_min;
    }
    else {
     // CORECTAT: Folosim noile tale variabile pentru ciclul de noapte
        total_ciclu = hydro_settings.water_night_on_min + hydro_settings.water_night_off_min;
        timp_on = hydro_settings.water_night_on_min;
    }

    if (total_ciclu <= 0) return;

    // Poziția matematică exactă în interiorul ciclului curent
    int pos_in_ciclu = minute_in_phase % total_ciclu;

    // 3. Calcul status pompă de APĂ
    bool new_pompa_apa = (pos_in_ciclu < timp_on);

    // 4. Calcul status pompă de AER (Aliniat dinamic cu noul total_ciclu)
    bool new_pompa_aer = false;

    if (hydro_settings.air_mode == 0) {
        new_pompa_aer = true; // Non-Stop 24/7
    }
    else if (hydro_settings.air_mode == 1) {
        new_pompa_aer = new_pompa_apa; // Sincron cu apa (urmărește automat și noaptea)
    }
    else if (hydro_settings.air_mode == 2) {
        // Pre-Start: pornește cu "air_pre_min" înainte de începerea slotului de apă
        int pre_start = (total_ciclu - hydro_settings.air_pre_min) % total_ciclu;
        new_pompa_aer = (pos_in_ciclu >= pre_start) || (pos_in_ciclu < timp_on);
    }

    // 5. Detectăm modificările de stare electrică și trimitem comanda pe releu
    bool changed = false;
    const char *reason = NULL;

    if (new_pompa_apa != pompa_apa_status) {
        pompa_apa_status = new_pompa_apa;
        switch_relay(RELAY_WATER_PUMP_GPIO, pompa_apa_status);
        changed = true;
        reason = pompa_apa_status ? (este_zi ? "Day - Water ON" : "Night - Water ON")
            : (este_zi ? "Day - Water OFF" : "Night - Water OFF");
    }

    if (new_pompa_aer != pompa_aer_status) {
        pompa_aer_status = new_pompa_aer;
        switch_relay(RELAY_AIR_PUMP_GPIO, pompa_aer_status);
        changed = true;

        if (reason == NULL) {
            reason = pompa_aer_status ? "Air loop started" : "Air loop ended";
        }
    }

    // Jurnalizare automată pe SD doar la schimbare
    if (changed) {
        struct tm tm = time_manager_get_current_time();
        storage_log_printf(LOG_TYPE_SYSTEM,
            "\n%02u:%02u:%02u - HYDRO [%s]: Water=%s | Air=%s | Reason: %s",
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            este_zi ? "DAY" : "NIGHT",
            pompa_apa_status ? "ON" : "OFF",
            pompa_aer_status ? "ON" : "OFF",
            reason ? reason : "unknown");
    }
}

// =============================================
// TASK-UL FREE RTOS ATASAT DE CORE 1
// =============================================
static void control_hydroponics_task(void *pvParameters) {
    ESP_LOGI(TAG, "Hydroponics control task started");
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        hydro_control_loop();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(60000)); // Execuție la fix 1 minut
    }
}

// =============================================
// PUBLIC API
// =============================================
esp_err_t hydro_manager_init(void) {
    event_bus_register_handler(SYSTEM_EVENTS, SYS_EVENT_STATE_CHANGED,
        hydro_state_event_handler, NULL);

    ESP_LOGI(TAG, "Hydro Manager initialized successfully ✓");
    return ESP_OK;
}

esp_err_t start_control_hydroponics_task(void) {
    // Îl păstrăm atașat hardware pe nucleul secundar (Core 1) conform codului tău original
    xTaskCreatePinnedToCore(control_hydroponics_task, "hydro_task", 4096, NULL, 5, NULL, 1);
    return ESP_OK;
}

bool hydro_is_water_pump_on(void) { return pompa_apa_status; }
bool hydro_is_air_pump_on(void) { return pompa_aer_status; }
