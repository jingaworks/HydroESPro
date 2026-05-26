#include "board.h"

static const char *TAG = "BOARD PREFERENCES";


/***********************************************/
/*** Board preferences *************************/
// Definirea instanței globale
state_manager_t sys_manager = {0};

// 1. Inițializarea statică globală cu valorile din fabrică (Default)
wifi_settings_t wifi_settings = {
    .wifi_mode = 0,                                 // 0 = Pornire directă în mod AP
    .sta_ssid = "",                                 // Goală inițial
    .sta_password = "",                             // Goală inițial
    .ap_ssid = "Garden_Assistant_AP",               // Nume rețea implicit din fabrică
    .ap_password = "12345678"                       // Parolă implicită (minim 8 caractere pentru WPA2)
};


env_settings_t env_settings = {0};

// Structura globală în care salvezi setările venite de pe pagina Web
// O inițializăm aici cu niște valori de test (Ex: Ziua începe la 06:00, ține 17h)
hydro_settings_t hydro_settings = {
    .day_period_hours = 17,
    .start_hour = 6,
    .start_minute = 0,
    .water_on_min = 10,         // 10 minute pornită // 5 minute pornită
    .water_off_min = 20,        // 20 minute oprită (Total ciclu = 30 min -> adică 2 cicluri/oră) // 10 minute oprit (Total ciclu = 15 min -> adică 4 cicluri/oră)
    .water_night_on_min = 5,    // Implicit: 5 minute pornită noaptea (NOU)
    .water_night_off_min = 60,  // Implicit: 60 minute oprită noaptea (NOU)
    .air_mode = 2,             // Modul 2: Pre-oxigenare
    .air_pre_min = 5           // Aerul pornește cu 5 minute înaintea apei // Aerul pornește cu 2 minute înaintea apei
};

alarm_settings_t alarm_settings = {
  .float_switch_inv = false,
  .mute_buzzer = false,
  .water_temp_min = 18.0,
  .water_temp_max = 26.5
};

log_settings_t log_settings = {
  .log_dht_enable = 1,
  .log_dht_interval = 5,
  .log_ds_enable = 1,
  .log_ds_interval = 5
};

/***********************************************/
/*** Preferences config ************************/
const char nvs_name_wifi[] = {"wifi_pref"};
esp_err_t erase_wifi_config(void)
{
    ESP_LOGI(TAG, "erase_wifi_config");
    nvs_handle_t pref_handle;
    esp_err_t err;

    err = nvs_open(nvs_name_wifi, NVS_READWRITE, &pref_handle);
    if (err != ESP_OK) return err;
    
    err = nvs_erase_all(pref_handle);
    if (err != ESP_OK) {
        nvs_close(pref_handle);
        return err;
    }
    
    nvs_close(pref_handle);
    return ESP_OK;
}
esp_err_t save_wifi_config(void)
{
    ESP_LOGI(TAG, "save_wifi_config");
    nvs_handle_t pref_handle;
    esp_err_t err;

    err = nvs_open(nvs_name_wifi, NVS_READWRITE, &pref_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error NVS open wifi %d", err);
        return err;
    }

    err = nvs_set_blob(pref_handle, "wifi_settings", &wifi_settings, sizeof(wifi_settings_t));
    if (err != ESP_OK) {
        nvs_close(pref_handle);
        ESP_LOGE(TAG, "Error saving wifi_settings blob %d", err);
        return err;
    }

    err = nvs_commit(pref_handle);
    if (err != ESP_OK) {
        nvs_close(pref_handle);
        ESP_LOGE(TAG, "Error committing wifi NVS %d", err);
        return err;
    }

    nvs_close(pref_handle);
    return ESP_OK;
}
esp_err_t load_wifi_config(void)
{
    ESP_LOGI(TAG, "load_wifi_config");
    nvs_handle_t pref_handle;
    esp_err_t err;
    size_t required_size;

    err = nvs_open(nvs_name_wifi, NVS_READWRITE, &pref_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error NVS open wifi %d", err);
        return err;
    }

    required_size = 0;
    err = nvs_get_blob(pref_handle, "wifi_settings", NULL, &required_size);
    if (required_size > 0) {
        err = nvs_get_blob(pref_handle, "wifi_settings", &wifi_settings, &required_size);
        if (err != ESP_OK) {
            nvs_close(pref_handle);
            ESP_LOGE(TAG, "Error loading wifi_settings %d. Auto-repairing...", err);
            save_wifi_config();
            return load_wifi_config();
        }
    }
    else {
        // Dacă nu există deloc partiția (la primul boot), scriem valorile default și re-citim
        nvs_close(pref_handle);
        ESP_LOGW(TAG, "Wifi config partition empty. Writing defaults...");
        save_wifi_config();
        return load_wifi_config();
    }

    nvs_close(pref_handle);
    return ESP_OK;
}



const char nvs_name_board[] = {"board_pref"};
esp_err_t erase_board_config(void)
{
  ESP_LOGI(TAG, "erase_board_config");

  nvs_handle_t pref_handle;
  esp_err_t err;

  // Open Board
  err = nvs_open(nvs_name_board, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK)
    return err;
  
  // Erase Board
  err = nvs_erase_all(pref_handle);
  if (err != ESP_OK)
    return err;
  
  // Close Board
  nvs_close(pref_handle);

  return ESP_OK;
}
esp_err_t save_board_config(void)
{
  ESP_LOGI(TAG, "save_board_config");

  nvs_handle_t pref_handle;
  esp_err_t err;

  // Open
  err = nvs_open(nvs_name_board, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error NVS open %d", err);
    return err;
  }

  // Write
  err = nvs_set_blob(pref_handle, "env_settings", &env_settings, sizeof(env_settings_t));
  if (err != ESP_OK) {
      nvs_close(pref_handle);
      ESP_LOGE(TAG, "Error env_settings %d", err);
      return err;
  }

  // Commit written value.
  err = nvs_commit(pref_handle);
  if (err != ESP_OK) {
    nvs_close(pref_handle);
    ESP_LOGE(TAG, "Error NVS commit %d", err);
    return err;
  }

  // Close
  nvs_close(pref_handle);
  return ESP_OK;
}
esp_err_t load_board_config(void)
{
  ESP_LOGI(TAG, "load_board_config");

  nvs_handle_t pref_handle;
  esp_err_t err;
  size_t required_size;

  // Open
  err = nvs_open(nvs_name_board, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error NVS open %d", err);
    return err;
  }

  // Read
  required_size = 0;
  err = nvs_get_blob(pref_handle, "env_settings", NULL, &required_size);
  if (required_size > 0) {
      err = nvs_get_blob(pref_handle, "env_settings", &env_settings, &required_size);
      if (err != ESP_OK) {
          nvs_close(pref_handle);
          ESP_LOGE(TAG, "Error env_settings %d", err);
          save_board_config();
          return load_board_config();
      }
  }
  else {
      nvs_close(pref_handle);
      save_board_config();
      return load_board_config();
  }

  // // Close
  nvs_close(pref_handle);
  return ESP_OK;
}


const char nvs_name_hydro[] = {"hydro_pref"};
esp_err_t erase_hydro_config(void)
{
  ESP_LOGI(TAG, "erase_hydro_config");

  nvs_handle_t pref_handle;
  esp_err_t err;

  // Open Board
  err = nvs_open(nvs_name_hydro, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK)
    return err;
  
  // Erase Board
  err = nvs_erase_all(pref_handle);
  if (err != ESP_OK)
    return err;
  
  // Close Board
  nvs_close(pref_handle);

  return ESP_OK;
}
esp_err_t save_hydro_config(void)
{
  ESP_LOGI(TAG, "save_hydro_config");

  nvs_handle_t pref_handle;
  esp_err_t err;

  // Open
  err = nvs_open(nvs_name_hydro, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error NVS open %d", err);
    return err;
  }

  // Write
  err = nvs_set_blob(pref_handle, "hydro_settings", &hydro_settings, sizeof(hydro_settings_t));
  if (err != ESP_OK) {
      nvs_close(pref_handle);
      ESP_LOGE(TAG, "Error env_settings %d", err);
      return err;
  }

  // Commit written value.
  err = nvs_commit(pref_handle);
  if (err != ESP_OK) {
    nvs_close(pref_handle);
    ESP_LOGE(TAG, "Error NVS commit %d", err);
    return err;
  }

  // Close
  nvs_close(pref_handle);
  return ESP_OK;
}
esp_err_t load_hydro_config(void)
{
  ESP_LOGI(TAG, "load_hydro_config");

  nvs_handle_t pref_handle;
  esp_err_t err;
  size_t required_size;

  // Open
  err = nvs_open(nvs_name_hydro, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error NVS open %d", err);
    return err;
  }

  // Read
  required_size = 0;
  err = nvs_get_blob(pref_handle, "hydro_settings", NULL, &required_size);
  if (required_size > 0) {
      err = nvs_get_blob(pref_handle, "hydro_settings", &hydro_settings, &required_size);
      if (err != ESP_OK) {
          nvs_close(pref_handle);
          ESP_LOGE(TAG, "Error hydro_settings %d", err);
          save_hydro_config();
          return load_hydro_config();
      }
  }
  else {
      nvs_close(pref_handle);
      save_hydro_config();
      return load_hydro_config();
  }

  // // Close
  nvs_close(pref_handle);
  return ESP_OK;
}

const char nvs_name_alarm[] = {"alarm_pref"};
esp_err_t erase_alarm_config(void)
{
  ESP_LOGI(TAG, "erase_alarm_config");

  nvs_handle_t pref_handle;
  esp_err_t err;

  // Open Board
  err = nvs_open(nvs_name_alarm, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK)
    return err;
  
  // Erase Board
  err = nvs_erase_all(pref_handle);
  if (err != ESP_OK)
    return err;
  
  // Close Board
  nvs_close(pref_handle);

  return ESP_OK;
}
esp_err_t save_alarm_config(void)
{
  ESP_LOGI(TAG, "save_alarm_config");  

  nvs_handle_t pref_handle;
  esp_err_t err;

  // Open
  err = nvs_open(nvs_name_alarm, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error NVS open %d", err);
    return err;
  }

  // Write
  err = nvs_set_blob(pref_handle, "alarm_settings", &alarm_settings, sizeof(alarm_settings_t));
  if (err != ESP_OK) {
      nvs_close(pref_handle);
      ESP_LOGE(TAG, "Error env_settings %d", err);
      return err;
  }

  // Commit written value.
  err = nvs_commit(pref_handle);
  if (err != ESP_OK) {
    nvs_close(pref_handle);
    ESP_LOGE(TAG, "Error NVS commit %d", err);
    return err;
  }

  // Close
  nvs_close(pref_handle);
  return ESP_OK;
}
esp_err_t load_alarm_config(void)
{
  ESP_LOGI(TAG, "load_alarm_config");

  nvs_handle_t pref_handle;
  esp_err_t err;
  size_t required_size;

  // Open
  err = nvs_open(nvs_name_alarm, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error NVS open %d", err);
    return err;
  }

  // Read
  required_size = 0;
  err = nvs_get_blob(pref_handle, "alarm_settings", NULL, &required_size);
  if (required_size > 0) {
      err = nvs_get_blob(pref_handle, "alarm_settings", &alarm_settings, &required_size);
      if (err != ESP_OK) {
          nvs_close(pref_handle);
          ESP_LOGE(TAG, "Error alarm_settings %d", err);
          save_alarm_config();
          return load_alarm_config();
      }
  }
  else {
      nvs_close(pref_handle);
      save_alarm_config();
      return load_alarm_config();
  }

  // // Close
  nvs_close(pref_handle);
  return ESP_OK;
}

const char nvs_name_log[] = {"log_pref"};
esp_err_t erase_log_config(void)
{
  ESP_LOGI(TAG, "erase_log_config");  

  nvs_handle_t pref_handle;
  esp_err_t err;

  // Open Board
  err = nvs_open(nvs_name_log, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK)
    return err;
  
  // Erase Board
  err = nvs_erase_all(pref_handle);
  if (err != ESP_OK)
    return err;
  
  // Close Board
  nvs_close(pref_handle);

  return ESP_OK;
}
esp_err_t save_log_config(void)
{
  ESP_LOGI(TAG, "save_log_config");  

  nvs_handle_t pref_handle;
  esp_err_t err;

  // Open
  err = nvs_open(nvs_name_log, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error NVS open %d", err);
    return err;
  }

  // Write
  err = nvs_set_blob(pref_handle, "log_settings", &log_settings, sizeof(log_settings_t));
  if (err != ESP_OK) {
      nvs_close(pref_handle);
      ESP_LOGE(TAG, "Error log_settings %d", err);
      return err;
  }

  // Commit written value.
  err = nvs_commit(pref_handle);
  if (err != ESP_OK) {
    nvs_close(pref_handle);
    ESP_LOGE(TAG, "Error NVS commit %d", err);
    return err;
  }

  // Close
  nvs_close(pref_handle);
  return ESP_OK;
}
esp_err_t load_log_config(void)
{
  ESP_LOGI(TAG, "load_log_config");

  nvs_handle_t pref_handle;
  esp_err_t err;
  size_t required_size;

  // Open
  err = nvs_open(nvs_name_log, NVS_READWRITE, &pref_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error NVS open %d", err);
    return err;
  }  

  // Read
  required_size = 0;
  err = nvs_get_blob(pref_handle, "log_settings", NULL, &required_size);
  if (required_size > 0) {
      err = nvs_get_blob(pref_handle, "log_settings", &log_settings, &required_size);
      if (err != ESP_OK) {
          nvs_close(pref_handle);
          ESP_LOGE(TAG, "Error log_settings %d", err);
          save_log_config();
          return load_log_config();
      }
  }
  else {
      nvs_close(pref_handle);
      save_log_config();
      return load_log_config();
  }

  // Close
  nvs_close(pref_handle);
  return ESP_OK;
}

void load_all_config(void) {
    load_wifi_config();
    load_board_config();
    load_hydro_config();
    load_alarm_config();
    load_log_config();
}