#pragma once

// Starts the HTTP server on port 80.
void startWebInterface();

// Serves pending client requests; call from loop().
void handleWebInterface();
