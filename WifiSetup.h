#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

String generateAPName();

bool wifiSetupBegin(const char* apName, int timeoutSeconds);


#endif
