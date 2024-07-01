#include "Arduino.h"
#include "main_project_utils.h"
#include "network_manager.h"
#include "user_interface.h"

// ---------- DEBUG AND SYSTEM CONTROL ---------- //

#define ENABLE_WIFI                             // enable wifi-module
#define ENABLE_MEMORY                           // enable memory-module
#define ENABLE_OTA                              // enable ota-module
#define ENABLE_GITHUBUPDATE                     // enable githubupdate-module
#define SYS_CONTROL_WEBSERIAL                   // enable the webserial


//#define SYS_CONTROL_STAT_IP                     // enable static ip

// set this to any password to protect the AP
#define AP_PASSWORD                     nullptr // a nullptr removes the password

// AP verbosity:
// 1 - AP will spawn if esp cannot connect to existing wifi for any reason
// 2 - AP will always spawn, even if connected to existing wifi
#define AP_VERBOSITY                    1

// comment this out to have an always on AP once its spawned
// #define AP_TIMEOUT                      300000      // 5 min
#define TIMEOUT_WIFI_CONNECT_MS         5000

// time module settings
#define TIMEZONE_SEC_OFFSET             3600  // CET is UTC+1
#define DAYLIGHT_SAVING_SEC_OFFSET      3600  // Additional offset for DST (CEST is UTC+2)

#define WIFI_SEARCH_INTERVAL            5000