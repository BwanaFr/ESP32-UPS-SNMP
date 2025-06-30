#ifndef _UPS_WEB_SERVER_H__
#define _UPS_WEB_SERVER_H__

#include <esp_https_server.h>


/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

/*
 * async send function, which we put into the httpd work queue
 */
void ws_async_send(void *arg);

esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req);
esp_err_t echo_handler(httpd_req_t *req);

void start_webserver(void);
void stop_webserver(void);

#endif