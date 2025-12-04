#include "api.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mbedtls/md.h"
#include <string.h>
#include "secrets.h"
#include "cJson.h"
#include "display.h"
static const char *TAG = "api";
int target_low_mgdl = 80;
int target_high_mgdl = 180;

static void sha1_hex(const char *input, char *output_hex)
{
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    unsigned char hash[20];

    mbedtls_md(md_info, (const unsigned char *)input, strlen(input), hash);

    for (int i = 0; i < 20; i++) {
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    }
    output_hex[40] = '\0';
}

void get_ranges(const char *host, const char *endpoint, const char *secret) 
{
    char url[256];
    snprintf(url, sizeof(url), "%s%s", host, endpoint);

    ESP_LOGI(TAG, "Requesting URL for ranges: %s", url);

    char hashedSecret[41];
    sha1_hex(secret, hashedSecret);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return;
    }

    esp_http_client_set_header(client, "api-secret", hashedSecret);
    esp_http_client_set_header(client, "Accept-Encoding", "identity;q=1, *;q=0");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    esp_http_client_fetch_headers(client);
    
    // --- Dynamic Memory Accumulation (Crucial for large JSON) ---
    char *response_buffer = NULL; // Pointer to the dynamically growing response body
    int total_read = 0;
    const int CHUNK_SIZE = 512;
    char chunk_buffer[CHUNK_SIZE];
    
    while (1) {
        int r = esp_http_client_read(client, chunk_buffer, sizeof(chunk_buffer));

        if (r < 0) {
            ESP_LOGE(TAG, "Error reading response body.");
            if (response_buffer) free(response_buffer);
            esp_http_client_cleanup(client);
            return;
        }
        if (r == 0) {
            break;
        }

        // Dynamically increase buffer size
        response_buffer = realloc(response_buffer, total_read + r + 1);
        if (response_buffer == NULL) {
            ESP_LOGE(TAG, "Memory reallocation failed.");
            esp_http_client_cleanup(client);
            return;
        }

        // Append new data
        memcpy(response_buffer + total_read, chunk_buffer, r);
        total_read += r;
        response_buffer[total_read] = '\0'; // Null-terminate
    }

    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status_code != 200 || !response_buffer) {
        ESP_LOGE(TAG, "Request failed (Status %d) or body is empty.", status_code);
        if (response_buffer) free(response_buffer);
        return;
    }

    cJSON *root = cJSON_Parse(response_buffer);
    free(response_buffer); 

    if (!root || !cJSON_IsArray(root) || cJSON_GetArraySize(root) == 0) {
        ESP_LOGE(TAG, "JSON parse failed or root is not a non-empty array. Using defaults.");
        return;
    }
    
    // 1. Get the first item (the profile object)
    cJSON *item = cJSON_GetArrayItem(root, 0); 
    if (!cJSON_IsObject(item)) { ESP_LOGE(TAG, "JSON error: First item is not an object."); goto cleanup; }

    // 2. Get 'store' from the item object (FIXED NAVIGATION)
    cJSON *store = cJSON_GetObjectItem(item, "store");
    if (!store || !cJSON_IsObject(store)) { 
        ESP_LOGE(TAG, "JSON error: 'store' object missing or invalid."); 
        goto cleanup; 
    }
    
    // 3. Get 'Default' profile
    cJSON *default_profile = cJSON_GetObjectItem(store, "Default");
    if (!default_profile || !cJSON_IsObject(default_profile)) { 
        ESP_LOGE(TAG, "JSON error: 'Default' profile object missing or invalid."); 
        goto cleanup; 
    }

    // 4. Get target arrays
    cJSON *target_low_arr = cJSON_GetObjectItem(default_profile, "target_low");
    cJSON *target_high_arr = cJSON_GetObjectItem(default_profile, "target_high");

    if (!target_low_arr || cJSON_GetArraySize(target_low_arr) == 0) {
        ESP_LOGE(TAG, "JSON error: target_low array missing or empty.");
        goto cleanup;
    }
    if (!target_high_arr || cJSON_GetArraySize(target_high_arr) == 0) {
        ESP_LOGE(TAG, "JSON error: target_high array missing or empty.");
        goto cleanup;
    }

    // 5. Get the 'value' from the first element of the target arrays
    cJSON *low_value_obj = cJSON_GetObjectItem(cJSON_GetArrayItem(target_low_arr, 0), "value");
    cJSON *high_value_obj = cJSON_GetObjectItem(cJSON_GetArrayItem(target_high_arr, 0), "value");

    if (!low_value_obj || !cJSON_IsNumber(low_value_obj) ||
        !high_value_obj || !cJSON_IsNumber(high_value_obj)) {
        
        ESP_LOGE(TAG, "JSON error: target 'value' not found or not a number.");
        goto cleanup;
    }
    
    set_ranges(low_value_obj->valueint, high_value_obj->valueint);
    
cleanup:
    cJSON_Delete(root);
}

void api_send_main_request(const char *host, const char *endpoint, const char *secret)
{
    char url[256];
    snprintf(url, sizeof(url), "%s%s", host, endpoint);

    ESP_LOGI(TAG, "Requesting URL: %s", url);

    char hashedSecret[41];
    sha1_hex(secret, hashedSecret);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return;
    }

    esp_http_client_set_header(client, "api-secret", hashedSecret);
    esp_http_client_set_header(client, "Accept-Encoding", "identity;q=1, *;q=0");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    int headers_len = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "Headers length: %d", headers_len);

    char buffer[512];
    int total = 0;

    while (1) {
        int r = esp_http_client_read(client, buffer, sizeof(buffer) - 1);
        if (r <= 0) break;
        buffer[r] = '\0';
        ESP_LOGI(TAG, "%s", buffer);
        total += r;
    }

    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse failed");
        return;
    }

    if (!cJSON_IsArray(root)) {
        ESP_LOGE(TAG, "Expected array");
        cJSON_Delete(root);
        return;
    }

    cJSON *item = cJSON_GetArrayItem(root, 0);
    if (!cJSON_IsObject(item)) {
        ESP_LOGE(TAG, "Expected object inside array");
        cJSON_Delete(root);
        return;
    }

    cJSON *sgv = cJSON_GetObjectItem(item, "sgv");
    if (!cJSON_IsNumber(sgv)) {
        ESP_LOGE(TAG, "sgv missing or not number");
        cJSON_Delete(root);
        return;
    }

    ESP_LOGI(TAG, "sgv: %d", sgv->valueint);

    cJSON *direction = cJSON_GetObjectItem(item, "direction");
    if (!cJSON_IsString(direction)) {
        ESP_LOGE(TAG, "direction missing");
        cJSON_Delete(root);
        return;
    }

    ESP_LOGI(TAG, "direction: %s", direction->valuestring);

    update_display(sgv->valueint, direction->valuestring);

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Total bytes read: %d", total);

    esp_http_client_cleanup(client);
}



void api_task(void *pvParameter)
{
    while (1) {
        api_send_main_request(API_HOST, API_ENDPOINT, API_SECRET);

        // Sleep for 90 seconds
        vTaskDelay(pdMS_TO_TICKS(90000));
    }
}
