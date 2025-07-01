#ifndef _UPS_WEB_SERVER_H__
#define _UPS_WEB_SERVER_H__

#include <esp_http_server.h>
#include <string>

class Webserver{
public:
    Webserver();
    virtual ~Webserver() = default;

    /**
     * Stars the Webserver
     */
    void start();

    /**
     * Stops the webserver
     */
    void stop();

    /**
     * Setup
     */
    void setup();

    /**
     * Sets credentials for protected pages
     */
    void setCredentials(const char* userName, const char* password);

    /**
     * Creates the authentication digest
     */
    static void createAuthDigest(std::string& digest, const char* usernane, const char* password);

private:
    std::string authDigest_;
    httpd_handle_t server_;

    /**
     * Checks authentication of the user
     * @param req HTTP request
     * @return true if authentication is success full
     */
    bool checkAuthentication(httpd_req_t *req);
    
    //HTTP handlers
    static esp_err_t ota_get_handler( httpd_req_t *req );       //Handle OTA GET request
    static esp_err_t ota_post_handler( httpd_req_t *req );      //Handle OTA POST request
    static esp_err_t cfg_get_handler( httpd_req_t *req );       //Handle configuration GET request
    static esp_err_t cfg_post_handler( httpd_req_t *req );      //Handle configuration POST request
};

extern Webserver webServer;
#endif