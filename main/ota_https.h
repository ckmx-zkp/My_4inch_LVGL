#pragma once
#include <stdbool.h>

void ota_https_confirm_app(void);

// Start HTTPS OTA on a background task. url must be https://...
bool ota_https_start(const char *url);

// -1 idle, 0..100 downloading, 101 applying/reboot.
int ota_https_progress(void);

const char *ota_https_error(void);
