#pragma once

#define WEB_SERVER_PORT 80

// Starts listening on WEB_SERVER_PORT. Call after wifiConnect() succeeds.
void webServerBegin();

// Serves at most one pending request; call from loop().
void webServerHandle();
