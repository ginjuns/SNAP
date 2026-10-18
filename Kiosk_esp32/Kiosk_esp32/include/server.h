#pragma once

void initWifi();
void ServerTask(void *pvParameters);
void sendToServer(const char *line);
