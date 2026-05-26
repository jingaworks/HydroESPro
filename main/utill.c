

#include "utill.h"

static const char *TAG = "UTILL";


void upTime(char *ret)
{
    char *p = ret;
    long days = 0;
    long hours = 0;
    long mins = 0;
    long secs = 0;
    secs = (esp_timer_get_time() / 1000) / 1000; // convect microseconds to seconds
    mins = secs / 60;                            // convert seconds to minutes
    hours = mins / 60;                           // convert minutes to hours
    days = hours / 24;                           // convert hours to days
    secs = secs - (mins * 60);                   // subtract the coverted seconds to minutes in order to display 59 secs max
    mins = mins - (hours * 60);                  // subtract the coverted minutes to hours in order to display 59 minutes max
    hours = hours - (days * 24);                 // subtract the coverted hours to days in order to display 23 hours max

    if (days)
    {
        p += sprintf(p, "%d", (char)days);
        p += sprintf(p, "%s", "d ");
    }

    if (hours)
    {
        p += sprintf(p, "%d", (char)hours);
        p += sprintf(p, "%s", "h ");
    }

    if (mins >= 0)
    {
        p += sprintf(p, "%d", (char)mins);
        p += sprintf(p, "%s", "m ");
    }

    if (secs >= 0)
    {
        p += sprintf(p, "%d", (char)secs);
        p += sprintf(p, "%s", "s");
    }

    *p++ = 0;
}

uint32_t get_time_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

size_t calloc_data(char **p, const char *msg, size_t size) {
    // size = size + 1;
    *p = calloc(size, sizeof(char));
    if (*p == NULL)
    {
        ESP_LOGE(TAG, "malloc fail");
        return 0;
    }
    memcpy(*p, msg, size);
    return size;
}
