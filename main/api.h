#pragma once

void api_send_request(const char *host, const char *endpoint, const char *secret);
void api_task(void *pvParameter);
void get_ranges(const char *host, const char *endpoint, const char *secret);