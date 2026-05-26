#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_ALARMS_IN_QUEUE 5

typedef struct {
    uint8_t alarm_id;       // ID unic pentru identificare (ex: 1 pentru apa calda, 2 pentru rezervor)
    uint32_t on_ms;         // Durata unui beep
    uint32_t off_ms;        // Pauza dintre beep-uri
    uint32_t beep_count;    // Număr de beep-uri per ciclu
    uint32_t pause_ms;      // Pauza lungă între cicluri
    uint32_t total_cycles;  // De câte ori se repetă alarma (0 = la infinit până la oprire manuală)
    uint8_t priority;       // Prioritate: 0 = Maximă, 1, 2, 3...
} buzzer_alarm_t;

// ID-uri unice pentru alarme
#define ID_APA_CALDA      1
#define ID_REZERVOR_GOL   2

bool buzzer_is_beeping(void);
bool buzzer_is_alarm_active(uint8_t alarm_id);

// Inițializează sistemul de alarme cu coadă
void buzzer_init(void);

// Adaugă o alarmă în coadă. Dacă este mai prioritară decât cea curentă, o suspendă instant pe cea veche
void buzzer_trigger_alarm(buzzer_alarm_t alarm);

// Elimină o alarmă specifică din coadă după ID-ul ei (util când se rezolvă o problemă a unui senzor)
void buzzer_clear_alarm_by_id(uint8_t alarm_id);

// Golește toată coada și oprește orice sunet (Mute de urgență din buton)
void buzzer_stop_all_alarms(void);

// Beep direct hardware (nu afectează coada de alarme, util pentru click de buton)
void buzzer_beep_direct(uint32_t duration_ms);

