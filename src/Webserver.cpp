#include <Webserver.hpp>
#include <Arduino.h>
#include "esp_log.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include <esp_flash_partitions.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <Configuration.hpp>
#include <UPSHIDDevice.hpp>
#include <Temperature.hpp>

#define DEVICE_NAME "ESP32"
#define HTTPD_401   "401 UNAUTHORIZED"           /*!< HTTP Response 401 */


static const char* TAG = "Webserver";
Webserver webServer;

extern const uint8_t ota_page_start[] asm("_binary_html_ota_html_start");
extern const uint8_t ota_page_end[] asm("_binary_html_ota_html_end");

//-----------------------------------------------------------------------------

bool Webserver::checkAuthentication(httpd_req_t *req)
{
    if(authDigest_.empty()){
        //Always authenticated
        return true;
    }
    char* auth_buffer = (char*)calloc(512, sizeof(char));
    if(!auth_buffer){
        ESP_LOGE(TAG, "Unable to allocate auth_buffer!");
    }
    size_t buf_len = httpd_req_get_hdr_value_len( req, "Authorization" ) + 1;
    if ( ( buf_len > 1 ) && ( buf_len <= 512 ) )
    {
        if ( httpd_req_get_hdr_value_str( req, "Authorization", auth_buffer, buf_len ) == ESP_OK )
        {
            if ( !strncmp(authDigest_.c_str(), auth_buffer, buf_len ) )
            {
                ESP_LOGI(TAG, "Authenticated!" );
                free(auth_buffer);              
                return true;
            }
        }
    }
    ESP_LOGI(TAG, "Not authenticated" );
    httpd_resp_set_status( req, HTTPD_401 );
    httpd_resp_set_hdr( req, "Connection", "keep-alive" );
    httpd_resp_set_hdr( req, "WWW-Authenticate", "Basic realm=\"UPS monitoring\"" );
    httpd_resp_send( req, NULL, 0 );
    free(auth_buffer);
    return false;
}

esp_err_t Webserver::ota_get_handler( httpd_req_t *req )
{
    Webserver* instance = static_cast<Webserver*>(req->user_ctx);

    if(instance->checkAuthentication(req)){
        httpd_resp_set_status( req, HTTPD_200 );
        httpd_resp_set_hdr( req, "Connection", "keep-alive" );
        httpd_resp_send( req, (const char*)ota_page_start, ota_page_end - ota_page_start);
        return ESP_OK;
    }
    return ESP_OK;
}

//-----------------------------------------------------------------------------
esp_err_t Webserver::ota_post_handler( httpd_req_t *req )
{
    Webserver* instance = static_cast<Webserver*>(req->user_ctx);
    if(instance->checkAuthentication(req)){

        char buf[256];
        httpd_resp_set_status( req, HTTPD_500 );    // Assume failure

        size_t ret, remaining = req->content_len;
        ESP_LOGI(TAG, "Receiving");

        esp_ota_handle_t update_handle = 0 ;
        const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        const esp_partition_t *running          = esp_ota_get_running_partition();
        esp_err_t err = ESP_OK;

        if ( update_partition == NULL )
        {
            ESP_LOGI(TAG, "Uh oh, bad things");
            goto return_failure;
        }

        ESP_LOGI(TAG, "Writing partition: type %d, subtype %d, offset 0x%08x", update_partition-> type, update_partition->subtype, update_partition->address);
        ESP_LOGI(TAG, "Running partition: type %d, subtype %d, offset 0x%08x", running->type, running->subtype, running->address);
        err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
        if (err != ESP_OK)
        {
            ESP_LOGI(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
            goto return_failure;
        }
        while ( remaining > 0 )
        {
            // Read the data for the request        //TODO: Review this 
            if ( ( ret = httpd_req_recv( req, buf, std::min( remaining, (size_t)sizeof( buf ) ) ) ) <= 0 )
            {
                if ( ret == HTTPD_SOCK_ERR_TIMEOUT )
                {
                // Retry receiving if timeout occurred
                continue;
                }

                goto return_failure;
            }

            size_t bytes_read = ret;

            remaining -= bytes_read;
            err = esp_ota_write( update_handle, buf, bytes_read);
            if (err != ESP_OK)
            {
                goto return_failure;
            }
        }

        ESP_LOGI(TAG, "Receiving done" );

        // End response
        if ( ( esp_ota_end(update_handle)                   == ESP_OK ) && 
            ( esp_ota_set_boot_partition(update_partition) == ESP_OK ) )
        {
            ESP_LOGI(TAG, "OTA Success?!");
            ESP_LOGI(TAG, "Rebooting" );

            httpd_resp_set_status( req, HTTPD_200 );
            httpd_resp_send( req, NULL, 0 );

            vTaskDelay( 2000 / portTICK_RATE_MS);
            esp_restart();

            return ESP_OK;
        }
        ESP_LOGI(TAG, "OTA End failed (%s)!", esp_err_to_name(err));

return_failure:
        if ( update_handle )
        {
            esp_ota_abort(update_handle);
        }

        httpd_resp_set_status( req, HTTPD_500 );    // Assume failure
        httpd_resp_send( req, NULL, 0 );
    }
    return ESP_FAIL;
}

esp_err_t Webserver::cfg_get_handler( httpd_req_t *req )
{
    Webserver* instance = static_cast<Webserver*>(req->user_ctx);

    if(instance->checkAuthentication(req)){
        httpd_resp_set_status( req, HTTPD_200 );
        httpd_resp_set_hdr( req, "Connection", "keep-alive" );
        httpd_resp_set_type(req, "application/json");
        std::string cfgJSON;
        Configuration.toJSONString(cfgJSON);
        httpd_resp_send( req, cfgJSON.c_str(), cfgJSON.length());
        return ESP_OK;
    }
    return ESP_OK;
}

esp_err_t Webserver::cfg_post_handler( httpd_req_t *req )
{
    Webserver* instance = static_cast<Webserver*>(req->user_ctx);
    if(instance->checkAuthentication(req)){
        size_t dataSize = std::min((size_t)512, req->content_len);
        std::string content;
        content.resize(dataSize);
        int ret = httpd_req_recv(req, &content[0], dataSize);
        if (ret <= 0) {  /* 0 return value indicates connection closed */
            /* Check if timeout occurred */
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* In case of timeout one can choose to retry calling
                * httpd_req_recv(), but to keep it simple, here we
                * respond with an HTTP 408 (Request Timeout) error */
                httpd_resp_send_408(req);
            }
            /* In case of error, returning ESP_FAIL will
            * ensure that the underlying socket is closed */
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "JSON (%u) : %s", dataSize, content.c_str());
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, content);
        if(error){
            httpd_resp_set_status( req, HTTPD_500 );    // Assume failure
            httpd_resp_send( req, "JSON parse failure", HTTPD_RESP_USE_STRLEN );
            return ESP_OK;
        }
        Configuration.fromJSON(doc);

        /* Send a simple response */
        const char resp[] = "Configuration updated";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

esp_err_t Webserver::status_get_handler( httpd_req_t *req )
{
    Webserver* instance = static_cast<Webserver*>(req->user_ctx);

    httpd_resp_set_status( req, HTTPD_200 );
    httpd_resp_set_hdr( req, "Connection", "keep-alive" );
    httpd_resp_set_type(req, "application/json");
    //Build a JSON with UPS status
    JsonDocument doc;
    //Sets UPS status to JSON file
    upsDevice.statusToJSON(doc);

    // //Adds some info from the configuration
    // std::string devName;
    // Configuration.getDeviceName(devName);
    // doc["Network"]["Name"] = devName;
    // IPAddress ipAddress, subnet, gateway;
    // Configuration.getIPAddress(ipAddress, subnet, gateway);
    // doc["Network"]["IP"] = ipAddress == INADDR_NONE ? "DHCP" : ipAddress.toString();
    // doc["Network"]["Subnet"] = subnet == INADDR_NONE ? "DHCP" : subnet.toString();
    // doc["Network"]["Gateway"] = gateway == INADDR_NONE ? "DHCP" : gateway.toString();

    // //Sets SNMP trap
    // IPAddress snmpTrap;
    // Configuration.getSNMPTrap(snmpTrap);
    // doc["SNMP"]["Trap IP"] = snmpTrap == INADDR_NONE ? "NONE" : snmpTrap.toString();

    //Adds temperature reading
    double temperature = tempProbe.getTemperature();
    if(temperature != DEVICE_DISCONNECTED_C){
        doc["Temperature"] = temperature;
    }

    std::string upsStatusJSON;
    serializeJson(doc, upsStatusJSON);
    httpd_resp_send( req, upsStatusJSON.c_str(), upsStatusJSON.length());
    return ESP_OK;
}

Webserver::Webserver() : server_(nullptr)
{
}

void Webserver::start()
{
    if(!server_){
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();

        // Start the httpd server
        ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
        if (httpd_start(&server_, &config) == ESP_OK) {
            //Register OTA get/post
            httpd_uri_t ota_post =
            {
                .uri       = "/ota",
                .method    = HTTP_POST,
                .handler   = ota_post_handler,
                .user_ctx  = this
            };
            httpd_register_uri_handler(server_, &ota_post);
            httpd_uri_t ota_get =
            {
                .uri       = "/ota",
                .method    = HTTP_GET,
                .handler   = ota_get_handler,
                .user_ctx  = this,
            };
            httpd_register_uri_handler(server_, &ota_get);

            //Configuration
            httpd_uri_t cfg_get =
            {
                .uri       = "/config",
                .method    = HTTP_GET,
                .handler   = cfg_get_handler,
                .user_ctx  = this,
            };
            httpd_register_uri_handler(server_, &cfg_get);

            httpd_uri_t cfg_post =
            {
                .uri       = "/config",
                .method    = HTTP_POST,
                .handler   = cfg_post_handler,
                .user_ctx  = this,
            };
            httpd_register_uri_handler(server_, &cfg_post);

            //Status
            httpd_uri_t status_get =
            {
                .uri       = "/status",
                .method    = HTTP_GET,
                .handler   = status_get_handler,
                .user_ctx  = this,
            };
            httpd_register_uri_handler(server_, &status_get);
            return;
        }
        ESP_LOGI(TAG, "Error starting server!");
        server_ = nullptr;
    }
}

void Webserver::stop()
{
    if(server_){
        httpd_stop(server_);
        server_ = nullptr;
    }
}

void Webserver::setup()
{

}

void Webserver::setCredentials(const char* userName, const char* password)
{
    createAuthDigest(authDigest_, userName, password);
}

void Webserver::createAuthDigest(std::string& digest, const char* usernane, const char* password)
{
    digest = "";
    if(usernane != nullptr){
        int rc = 0;
        char* user_info;
        if(password != nullptr){
            rc = asprintf(&user_info, "%s:%s", usernane, password);
        }else{
            rc = asprintf(&user_info, "%s:", usernane);
        }
        if(rc < 0){
            ESP_LOGE(TAG, "asprintf() returned: %d", rc);
            return;
        }
        if (!user_info) {
            ESP_LOGE(TAG, "No enough memory for user information");
            return;
        }
        size_t n = 0;
        //Get len of the base64 string
        esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));
        char * digestCStr = (char*)calloc(1, 6 + n + 1);
        if(digestCStr) {
            strcpy(digestCStr, "Basic ");
            size_t out;
            esp_crypto_base64_encode((unsigned char *)digestCStr + 6, n, &out, (const unsigned char *)user_info, strlen(user_info));
            digest = digestCStr;
            free(digestCStr);
        }
        free(user_info);
    }
}
