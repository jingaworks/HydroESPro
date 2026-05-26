#include "http_server.h"
#include "rtc_ds3231.h"

static const char *TAG = "LOCAL_SERVER";


// http
const size_t max_clients = 4;
const size_t max_ws_open_sockets = 4;

// task handle
TaskHandle_t WEB_SERVER_TaskHandle;
TaskHandle_t WEB_STATUS_TaskHandle;

// data queue
QueueHandle_t web_data_queue;

// http server
httpd_handle_t server = NULL;

// http request
const char *field = "Connection";
const char *value = "close";


// static function declarations
static void ws_send_async(void *arg);

static bool check_client_alive_cb(ws_keep_alive_t h, int fd);
static bool client_not_alive_cb(ws_keep_alive_t h, int fd);

static esp_err_t http_local_server(void);

static void web_server_task(void *pvParameters);
static void web_status_task(void *pvParameters);


// queue
static esp_err_t init_data_queue(void) {
    web_data_queue = xQueueCreate(WEB_DATA_QUEUE_SIZE, sizeof(struct ws_web_data_t *));
    if (web_data_queue == NULL) {
        ESP_LOGE(TAG, "xQueueCreate: web_data_queue => Fail");
        return ESP_FAIL;
    }
    return ESP_OK;
}


/* Scratch buffer size */
#define SCRATCH_BUFSIZE  1024

struct file_server_data {
    /* Base path of file storage */
    char base_path[128];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize) {
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename) {
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    }
    else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    }
    else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories */
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath) {
    char entrypath[255];
    struct dirent *entry;
    DIR *dir = opendir(dirpath);

    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    // 1. Setați tipul de conținut ca JSON pentru ca browserul să îl proceseze corect
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");

    bool first_item = true;
    while ((entry = readdir(dir)) != NULL) {
        // Filtrăm doar fișierele .txt de log
        if (strstr(entry->d_name, ".txt") != NULL) {
            char json_line[300]; // Dimensiune sigură pentru a evita eroarea de compilare

            if (first_item) {
                snprintf(json_line, sizeof(json_line), "\"%s\"", entry->d_name);
                first_item = false;
            }
            else {
                snprintf(json_line, sizeof(json_line), ",\"%s\"", entry->d_name);
            }
            httpd_resp_sendstr_chunk(req, json_line);
        }
    }
    closedir(dir);

    // 2. Închidem tabloul JSON
    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL); // Finalizare răspuns HTTP Chunked

    // ESP_LOGI(TAG, "Lista de fisiere trimisa ca JSON cu succes.");
    return ESP_OK;
}
/* Handler to download files kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req) {
    char filepath[255];
    FILE *fd = NULL;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
        req->uri, sizeof(filepath));
    // ESP_LOGI(TAG, "URI: %s", req->uri);
    // ESP_LOGI(TAG, "Filename: %s", filename);

    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/') {
        // ESP_LOGI(TAG, "Directory listing requested");
        return http_resp_dir_html(req, filepath);
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Sending file : %s...", filename);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGD(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
// #ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, field, value);
// #endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t *req) {
    char filepath[255];;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    // ESP_LOGW(TAG, "URI: %s", req->uri);

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
        req->uri + sizeof("/delete") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/logs/");
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

// handler modificat
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        // ESP_LOGI(TAG, "New connection opened");

        // Trimitem automat comanda "init" în coadă la deschiderea conexiunii
        if (web_data_queue != 0) {
            struct ws_web_data_t *init_web = calloc(1, sizeof(struct ws_web_data_t));
            if (init_web != NULL) {
                init_web->data = calloc(1, 5); // "init" + NULL terminator
                if (init_web->data != NULL) {
                    strcpy((char *)init_web->data, "init");
                    init_web->len = 4;

                    // Trimitem în coadă asincron
                    if (xQueueSend(web_data_queue, (void *)&init_web, WEB_MAXDELAY) != pdTRUE) {
                        ESP_LOGW(TAG, "Send init to queue fail");
                        free(init_web->data);
                        free(init_web);
                    }
                }
                else {
                    free(init_web);
                }
            }
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // First receive the full ws message
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    if (ws_pkt.len) {
        // ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
        // If it was a PONG, update the keep-alive
        ESP_LOGD(TAG, "Received PONG message");
        free(buf);
        return ws_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle), httpd_req_to_sockfd(req));
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT || ws_pkt.type == HTTPD_WS_TYPE_PING || ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && ws_pkt.len <= WEB_DATA_MAX_SIZE) {
            // ESP_LOGI(TAG, "Received packet with message: %s", ws_pkt.payload);
            if (ws_pkt.len) {
                // process payload
                struct ws_web_data_t *recv_web;
                recv_web = calloc(1, sizeof(struct ws_web_data_t)); // CORECTAT: sizeof(struct ws_web_data_t) pentru siguranță explicită
                if (recv_web == NULL) {
                    ESP_LOGE(TAG, "Calloc receive recv_web fail");
                    free(buf); // REPARAT: leak buffer primit
                    return ESP_FAIL;
                }

                recv_web->data = calloc(1, ws_pkt.len + 1);
                if (recv_web->data == NULL) {
                    ESP_LOGE(TAG, "Calloc receive recv_web->data fail");
                    free(recv_web);
                    free(buf); // REPARAT: leak buffer primit
                    return ESP_FAIL;
                }
                memcpy(recv_web->data, ws_pkt.payload, ws_pkt.len);

                recv_web->len = ws_pkt.len;
                if (xQueueSend(web_data_queue, (void *)&recv_web, WEB_MAXDELAY) != pdTRUE) {
                    ESP_LOGW(TAG, "Send receive queue fail");
                    free(recv_web->data);
                    free(recv_web);
                }
            }
        }
        else if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
            // Response PONG packet to peer
            // ESP_LOGI(TAG, "Got a WS PING frame, Replying PONG");
            ws_pkt.type = HTTPD_WS_TYPE_PONG;
        }
        else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
            // Response CLOSE packet with no payload to peer
            ws_pkt.len = 0;
            ws_pkt.payload = NULL;
        }

        free(buf);
        return ret;
    }
    free(buf);
    return ESP_OK;
}
static esp_err_t favicon_get_handler(httpd_req_t *req) {
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_set_hdr(req, field, value);
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}
static esp_err_t style_get_handler(httpd_req_t *req) {
    // ESP_LOGI(TAG, "Serve style");
    extern const char style_start[] asm("_binary_style_css_start");
    extern const char style_end[] asm("_binary_style_css_end");
    const uint32_t style_len = style_end - style_start;
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, style_start, style_len);
    return ESP_OK;
}
static esp_err_t js_get_handler(httpd_req_t *req) {
    // ESP_LOGI(TAG, "Serve js");
    extern const char js_start[] asm("_binary_script_js_start");
    extern const char js_end[] asm("_binary_script_js_end");
    const uint32_t js_len = js_end - js_start;
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, js_start, js_len);
    return ESP_OK;
}
static esp_err_t index_get_handler(httpd_req_t *req) {
    // ESP_LOGI(TAG, "Serve index");
    extern const char index_start[] asm("_binary_index_html_start");
    extern const char index_end[] asm("_binary_index_html_end");
    const uint32_t index_len = index_end - index_start;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, field, value);
    httpd_resp_send(req, index_start, index_len);
    return ESP_OK;
}

// uri
static const httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = true };

static const httpd_uri_t icon = {
    .uri = "/favicon.ico",
    .method = HTTP_GET,
    .handler = favicon_get_handler };

static const httpd_uri_t style = {
    .uri = "/style.css",
    .method = HTTP_GET,
    .handler = style_get_handler };

static const httpd_uri_t js = {
    .uri = "/script.js",
    .method = HTTP_GET,
    .handler = js_get_handler };

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler };

// ws 
static esp_err_t ws_open_fd(httpd_handle_t hd, int sockfd) {
    // ESP_LOGI(TAG, "New client connected %d", sockfd);
    ws_keep_alive_t h = httpd_get_global_user_ctx(hd);
    return ws_keep_alive_add_client(h, sockfd);
}
static void ws_close_fd(httpd_handle_t hd, int sockfd) {
    // ESP_LOGI(TAG, "Client disconnected %d", sockfd);
    ws_keep_alive_t h = httpd_get_global_user_ctx(hd);
    ws_keep_alive_remove_client(h, sockfd);
    close(sockfd);
}
static void ws_send_async(void *arg) {
    struct ws_async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = resp_arg->data;
    ws_pkt.len = resp_arg->len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg->data);
    free(resp_arg);
}
static void send_ping(void *arg) {
    struct ws_async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = NULL;
    ws_pkt.len = 0;
    ws_pkt.type = HTTPD_WS_TYPE_PING;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

// ws client_alive cb
static bool check_client_alive_cb(ws_keep_alive_t h, int fd) {
    ESP_LOGD(TAG, "Checking if client (fd=%d) is alive", fd);
    struct ws_async_resp_arg *resp_arg = malloc(sizeof(struct ws_async_resp_arg));
    resp_arg->hd = ws_keep_alive_get_user_ctx(h);
    resp_arg->fd = fd;

    if (httpd_queue_work(resp_arg->hd, send_ping, resp_arg) == ESP_OK) {
        return true;
    }

    return false;
}
static bool client_not_alive_cb(ws_keep_alive_t h, int fd) {
    // ESP_LOGE(TAG, "Client not alive, closing fd %d", fd);
    httpd_sess_trigger_close(ws_keep_alive_get_user_ctx(h), fd);
    return true;
}

// http
static esp_err_t http_local_server(void) {
    // ESP_LOGI(TAG, "http_local_server: start http_server");

    static struct file_server_data *server_data = NULL;

    if (server_data) {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, MOUNT_POINT_SD, sizeof(server_data->base_path));

    // Prepare keep-alive engine
    ws_keep_alive_config_t keep_alive_config = KEEP_ALIVE_CONFIG_DEFAULT();
    keep_alive_config.max_clients = max_clients;
    keep_alive_config.client_not_alive_cb = client_not_alive_cb;
    keep_alive_config.check_client_alive_cb = check_client_alive_cb;
    ws_keep_alive_t keep_alive = ws_keep_alive_start(&keep_alive_config);

    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    conf.max_uri_handlers = 10;
    conf.max_open_sockets = max_ws_open_sockets;
    conf.global_user_ctx = keep_alive;
    conf.open_fn = ws_open_fd;
    conf.close_fn = ws_close_fd;
    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    conf.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t ret = httpd_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "Error starting http_server! error %s", esp_err_to_name(ret));
        ws_keep_alive_stop(keep_alive);
        return ESP_FAIL;
    }

    // Set URI handlers
    httpd_register_uri_handler(server, &ws);
    httpd_register_uri_handler(server, &icon);
    httpd_register_uri_handler(server, &style);
    httpd_register_uri_handler(server, &js);
    httpd_register_uri_handler(server, &root);

    httpd_uri_t file_download = {
        .uri = "/*",  // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = server_data    // Pass server data as context
    };

    httpd_uri_t file_delete = {
          .uri = "/delete/*",   // Match all URIs of type /delete/path/to/file
          .method = HTTP_POST,
          .handler = delete_post_handler,
          .user_ctx = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);
    httpd_register_uri_handler(server, &file_delete);


    ws_keep_alive_set_user_ctx(keep_alive, server);

    // ESP_LOGI(TAG, "http_local_server: http_server started.");

    return ESP_OK;
}
esp_err_t start_http_server(void) {
    // ESP_LOGI(TAG, "start_http_server");

    init_data_queue();

    esp_err_t ret = http_local_server();
    if (ret == ESP_OK) {
        start_server_task();
        start_status_task();
    }

    return ret;
}
void stop_server(void) {
    // Stop keep alive thread
    ws_keep_alive_stop(httpd_get_global_user_ctx(server));
    // Stop the httpd server
    httpd_stop(server);
}

/*################################################
  ### TASKS
  ################################################*/
static void web_server_task(void *pvParameters) {
    // ESP_LOGI(TAG, "Start: web_server_task");

    for (;;) {
        if (web_data_queue != 0) {
            struct ws_web_data_t *web_data;

            if (xQueueReceive(web_data_queue, &web_data, portMAX_DELAY)) {
                char buf[128] = { 0 };
                char variable[64] = { 0 };
                char value[64] = { 0 };
                int uint_val = 0;

                memcpy(buf, web_data->data, web_data->len);

                free(web_data->data);
                free(web_data);

                // Încercăm să parsăm ca JSON (formatul trimis de noul script.js)
                cJSON *root = cJSON_Parse(buf);
                if (root != NULL) {
                    cJSON *id_item = cJSON_GetObjectItem(root, "id");
                    cJSON *val_item = cJSON_GetObjectItem(root, "value");

                    if (cJSON_IsString(id_item) && id_item->valuestring) {
                        strncpy(variable, id_item->valuestring, sizeof(variable) - 1);
                        printf("variable from JSON: %s\n", variable);
                    }

                    if (cJSON_IsString(val_item) && val_item->valuestring) {
                        strncpy(value, val_item->valuestring, sizeof(value) - 1);
                        uint_val = atoi(value);
                        printf("value from JSON (string): %s\n", value);
                    }
                    else if (cJSON_IsNumber(val_item)) {
                        uint_val = val_item->valueint;
                        snprintf(value, sizeof(value), "%d", uint_val);
                        printf("value from JSON (number): %d\n", uint_val);
                    }
                    cJSON_Delete(root);
                }
                else {
                    // Dacă nu este JSON, aplicăm logica veche cu strtok (ex: pentru comenzi text simple sau "init")
                    char *token = strtok(buf, "=");
                    if (token != NULL) {
                        strncpy(variable, token, sizeof(variable) - 1);
                        printf("variable from strtok: %s\n", variable);

                        token = strtok(NULL, "");
                        if (token != NULL) {
                            strncpy(value, token, sizeof(value) - 1);
                            uint_val = atoi(value);
                            printf("value from strtok: %s\n", value);
                        }
                    }
                    else {
                     // Caz de siguranță: dacă vine doar un text simplu fără "=" (ex: "init" sau "reboot")
                        strncpy(variable, buf, sizeof(variable) - 1);
                        printf("variable direct string: %s\n", variable);
                    }
                }

                // --- LOGICĂ PROCESARE COMENZI ȘI SALVARE SETĂRI ---

                // Reset board
                if (!strcmp(variable, "reboot")) {
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                    esp_restart();
                }
                else if (!strcmp(variable, "sync_browser_time")) {
                    // Verificăm dacă sistemul este deja sincronizat prin SNTP (Internet). 
                    // Dacă are deja internet, prioritizăm SNTP-ul și ignorăm timpul browserului pentru a evita decalaje fine de milisecunde.
                    if (!time_manager_is_synced()) {
                        time_t browser_unix_sec = (time_t)strtol(value, NULL, 10);

                        if (browser_unix_sec > 1700000000) { // Validare minimă de siguranță (anul > 2023)
                            struct timeval tv_browser = { .tv_sec = browser_unix_sec, .tv_usec = 0 };

                            ESP_LOGW(TAG, "Sistemul nu are internet. Se aplică ora primită de la browser: %ld", (long)browser_unix_sec);

                            // Apelăm funcția ta nativă care salvează timpul în DS3231 și actualizează ceasul intern
                            if (set_ds_browser_time(&tv_browser) == ESP_OK) {
                                ESP_LOGI(TAG, "Ora sistemului și modulul RTC DS3231 au fost sincronizate cu succes din browser!");

                                // Opțional: Forțăm o notificare pe WebSocket înapoi la pagină ca să actualizeze instant ceasul web
                                // web_server_send_time_update();
                            }
                            else {
                                ESP_LOGE(TAG, "Eroare la scrierea timpului în modulul hardware DS3231.");
                            }
                        }
                    }
                    else {
                        ESP_LOGI(TAG, "Comandă timp browser primită, dar ignorată deoarece sistemul este deja aliniat stabil prin SNTP.");
                    }
                }
                                // Setări noul hydro_settings_t
                else if (!strcmp(variable, "hydro_start_h")) {
                    if (uint_val >= 0 && uint_val <= 23) {
                        // ESP_LOGI(TAG, "hydro start hour: %d", uint_val);
                        hydro_settings.start_hour = uint_val;
                        save_hydro_config();
                    }
                }
                else if (!strcmp(variable, "hydro_start_m")) {
                    if (uint_val >= 0 && uint_val <= 59) {
                        // ESP_LOGI(TAG, "hydro start minute: %d", uint_val);
                        hydro_settings.start_minute = uint_val;
                        save_hydro_config();
                    }
                }
                else if (!strcmp(variable, "hydro_period_hours")) {
                    if (uint_val >= 1 && uint_val <= 24) {
                        // ESP_LOGI(TAG, "hydro day period hours: %d", uint_val);
                        hydro_settings.day_period_hours = uint_val;
                        save_hydro_config();
                    }
                }
                else if (!strcmp(variable, "hydro_water_on")) {
                    if (uint_val >= 0 && uint_val <= 60) {
                        // ESP_LOGI(TAG, "hydro water ON min: %d", uint_val);
                        hydro_settings.water_on_min = uint_val;
                        save_hydro_config();
                    }
                }
                else if (!strcmp(variable, "hydro_water_off")) {
                    if (uint_val >= 0 && uint_val <= 60) {
                        // ESP_LOGI(TAG, "hydro water OFF min: %d", uint_val);
                        hydro_settings.water_off_min = uint_val;
                        save_hydro_config();
                    }
                }
                // 1. În interiorul web_server_task, unde procesezi setările primite, adaugă:
                else if (!strcmp(variable, "hydro_w_night_on")) {
                    if (uint_val >= 0 && uint_val <= 60) {
                        ESP_LOGI(TAG, "hydro water NIGHT on min: %d", uint_val);
                        hydro_settings.water_night_on_min = uint_val;
                        save_hydro_config();
                    }
                }
                else if (!strcmp(variable, "hydro_w_night_off")) {
                    if (uint_val >= 0 && uint_val <= 120) { // Noaptea lăsăm o limită mai mare, de ex 2 ore
                        ESP_LOGI(TAG, "hydro water NIGHT off min: %d", uint_val);
                        hydro_settings.water_night_off_min = uint_val;
                        save_hydro_config();
                    }
                }
                else if (!strcmp(variable, "hydro_air_mode")) {
                    if (uint_val >= 0 && uint_val <= 2) {
                        // ESP_LOGI(TAG, "hydro air mode: %d", uint_val);
                        hydro_settings.air_mode = uint_val;
                        save_hydro_config();
                    }
                }
                else if (!strcmp(variable, "hydro_air_pre_min")) {
                    if (uint_val >= 0 && uint_val <= 60) {
                        // ESP_LOGI(TAG, "hydro air pre min: %d", uint_val);
                        hydro_settings.air_pre_min = uint_val;
                        save_hydro_config();
                    }
                }

                // Setări noul alarm_settings_t
                else if (!strcmp(variable, "alarm_mute")) {
                    // ESP_LOGI(TAG, "alarm mute: %d", uint_val);
                    alarm_settings.mute_buzzer = (uint_val == 1);
                    save_alarm_config();
                }
                // Alarm - Temp Water MIN (Atenție, fiind float folosim atof în loc de uint_val!)
                else if (!strcmp(variable, "alarm_t_min")) {
                    float float_val = atof(value);
                    if (float_val >= 5.0 && float_val <= 35.0) {
                        // ESP_LOGI(TAG, "alarm temp min: %.1f", float_val);
                        alarm_settings.water_temp_min = float_val;
                        save_alarm_config();
                    }
                }
                // Alarm - Temp Water MAX
                else if (!strcmp(variable, "alarm_t_max")) {
                    float float_val = atof(value);
                    if (float_val >= 15.0 && float_val <= 45.0) {
                        // ESP_LOGI(TAG, "alarm temp max: %.1f", float_val);
                        alarm_settings.water_temp_max = float_val;
                        save_alarm_config();
                    }
                }
                // Alarm - Inversare logică senzor bazin
                else if (!strcmp(variable, "alarm_float_inv")) {
                    // ESP_LOGI(TAG, "alarm float invert: %d", uint_val);
                    alarm_settings.float_switch_inv = (uint_val == 1);
                    save_alarm_config();
                }

                // Setare activare log dht
                else if (!strcmp(variable, "log_dht_enable")) {
                    if (uint_val == 0 || uint_val == 1) {
                        ESP_LOGI(TAG, "Log dht enable: %d", uint_val);
                        log_settings.log_dht_enable = uint_val;
                        save_log_config(); // sau save_storage_config();
                    }
                }
                // Setare interval log dht
                else if (!strcmp(variable, "log_dht_interval")) {
                    if (uint_val >= 1 && uint_val <= 60) {
                        ESP_LOGI(TAG, "Log dht interval: %d min", uint_val);
                        log_settings.log_dht_interval = uint_val;
                        save_log_config();
                    }
                }
                // Setare activare log ds
                else if (!strcmp(variable, "log_ds_enable")) {
                    if (uint_val == 0 || uint_val == 1) {
                        ESP_LOGI(TAG, "Log ds enable: %d", uint_val);
                        log_settings.log_ds_enable = uint_val;
                        save_log_config();
                    }
                }
                // Setare interval log ds
                else if (!strcmp(variable, "log_ds_interval")) {
                    if (uint_val >= 1 && uint_val <= 60) {
                        ESP_LOGI(TAG, "Log ds interval: %d min", uint_val);
                        log_settings.log_ds_interval = uint_val;
                        save_log_config();
                    }
                }

                                // --- TRIMITERE RĂSPUNS CĂTRE CLIENȚI (CONSTRUIRE JSON VALID) ---

                size_t clients = max_clients;
                int client_fds[max_clients];

                if (httpd_get_client_list(server, &clients, client_fds) == ESP_OK) {
                    bool init_data = (!strcmp(variable, "init"));

                    static char json_response[(1024 * 6)];
                    char *p = json_response;
                    if (init_data) {
                        *p++ = '{';

                        // Telemetrie de pornire
                        // DHT Readings (Numele cheilor modificate pentru a se potrivi cu ID-urile din noul HTML)
                        if (dht_valid_reading) {
                            p += sprintf(p, "\"temperature\":\"%.1f\",", dht_temp);
                            p += sprintf(p, "\"humidity\":\"%.1f\",", dht_humid);
                        }
                        else {
                            p += sprintf(p, "\"temperature\":\"--.-\",\"humidity\":\"--\",");
                        }

                        // Setări hidro (Cheile trebuie să fie identice cu ID-urile din HTML!)
                        p += sprintf(p, "\"hydro_start_h\":\"%02d\",", hydro_settings.start_hour);
                        p += sprintf(p, "\"hydro_start_m\":\"%02d\",", hydro_settings.start_minute);
                        p += sprintf(p, "\"hydro_period_hours\":\"%d\",", hydro_settings.day_period_hours);
                        
                        p += sprintf(p, "\"hydro_water_on\":\"%d\",", hydro_settings.water_on_min);
                        p += sprintf(p, "\"hydro_water_off\":\"%d\",", hydro_settings.water_off_min);
                        p += sprintf(p, "\"hydro_w_night_on\":\"%d\",", hydro_settings.water_night_on_min);
                        p += sprintf(p, "\"hydro_w_night_off\":\"%d\",", hydro_settings.water_night_off_min);

                        p += sprintf(p, "\"hydro_air_mode\":\"%d\",", hydro_settings.air_mode);
                        p += sprintf(p, "\"hydro_air_pre_min\":\"%d\",", hydro_settings.air_pre_min);

                        // Setări alarme noi
                        p += sprintf(p, "\"alarm_mute\":\"%d\",", alarm_settings.mute_buzzer ? 1 : 0);
                        p += sprintf(p, "\"alarm_t_min\":\"%.1f\",", alarm_settings.water_temp_min);
                        p += sprintf(p, "\"alarm_t_max\":\"%.1f\",", alarm_settings.water_temp_max);
                        p += sprintf(p, "\"alarm_float_inv\":\"%d\",", alarm_settings.float_switch_inv ? 1 : 0); // Ultimul fără virgulă

                        // Setări log
                        p += sprintf(p, "\"log_dht_enable\":\"%d\",", log_settings.log_dht_enable);
                        p += sprintf(p, "\"log_dht_interval\":\"%d\",", log_settings.log_dht_interval);
                        p += sprintf(p, "\"log_ds_enable\":\"%d\",", log_settings.log_ds_enable);
                        p += sprintf(p, "\"log_ds_interval\":\"%d\"", log_settings.log_ds_interval);

                        *p++ = '}';
                        *p++ = 0;
                    }
                    if (init_data) {
                        // ESP_LOGI(TAG, "sending initial data...");
                        *p++ = '{';

                        // DHT 1 Readings (Numele cheilor modificate pentru a se potrivi cu ID-urile din noul HTML)
                        if (dht_valid_reading) {
                            p += sprintf(p, "\"temperature\":\"%.1f\",", dht_temp);
                            p += sprintf(p, "\"humidity\":\"%.1f\",", dht_humid);
                        }
                        else {
                            p += sprintf(p, "\"temperature\":\"--.-\",\"humidity\":\"--\",");
                        }

                        // Trimitem și valorile curente ale noilor inputuri la inițializare pentru a se completa pe pagină
                        p += sprintf(p, "\"hydro_start_h\":\"%02d\",", hydro_settings.start_hour);
                        p += sprintf(p, "\"hydro_start_m\":\"%02d\",", hydro_settings.start_minute);
                        p += sprintf(p, "\"hydro_period_hours\":\"%d\",", hydro_settings.day_period_hours);
                        p += sprintf(p, "\"hydro_water_on\":\"%d\",", hydro_settings.water_on_min);
                        p += sprintf(p, "\"hydro_water_off\":\"%d\",", hydro_settings.water_off_min);
                        p += sprintf(p, "\"hydro_air_mode\":\"%d\",", hydro_settings.air_mode);
                        p += sprintf(p, "\"hydro_air_pre_min\":\"%d\"", hydro_settings.air_pre_min); // Ultimul element NU are virgulă la sfârșit

                        p += sprintf(p, "\"alarm_mute\":\"%d\",", alarm_settings.mute_buzzer ? 1 : 0);
                        p += sprintf(p, "\"alarm_t_min\":\"%.1f\",", alarm_settings.water_temp_min);
                        p += sprintf(p, "\"alarm_t_max\":\"%.1f\",", alarm_settings.water_temp_max);
                        p += sprintf(p, "\"alarm_float_inv\":\"%d\",", alarm_settings.float_switch_inv ? 1 : 0);

                        *p++ = '}';
                        *p++ = 0;
                    }
                    else {
                        // Răspuns curat JSON trimis înapoi către browser la fiecare schimbare a unui singur input
                        *p++ = '{';
                        p += sprintf(p, "\"id\":\"%s\",\"value\":\"%s\"", variable, value);
                        *p++ = '}';
                        *p++ = 0;
                    }

                    // Expediere pachet asincron prin WebSocket
                    for (size_t i = 0; i < clients; ++i) {
                        int sock = client_fds[i];
                        if (httpd_ws_get_fd_info(server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
                            struct ws_async_resp_arg *resp_arg = calloc(1, sizeof(struct ws_async_resp_arg));
                            if (resp_arg == NULL) {
                                ESP_LOGE(TAG, "Calloc receive resp_arg fail");
                                break;
                            }
                            resp_arg->data = calloc(1, strlen(json_response) + 1);
                            if (resp_arg->data == NULL) {
                                ESP_LOGE(TAG, "Calloc receive resp_arg data fail");
                                free(resp_arg);
                                break;
                            }
                            memcpy(resp_arg->data, json_response, strlen(json_response));
                            resp_arg->len = strlen(json_response);
                            resp_arg->hd = server;
                            resp_arg->fd = sock;
                            if (httpd_queue_work(resp_arg->hd, ws_send_async, resp_arg) != ESP_OK) {
                                ESP_LOGE(TAG, "httpd_queue_work failed!");
                                free(resp_arg->data);
                                free(resp_arg);
                                break;
                            }
                        }
                    }
                }
                else {
                    ESP_LOGE(TAG, "httpd_get_client_list failed!");
                }
            }
        }
    }
}
bool citeste_senzor_nivel() {
    return gpio_get_level(WARNING_LED_GPIO);
}
static void web_status_task(void *pvParameters) {
    // ESP_LOGI(TAG, "Start: web_status_task (Mod: 5 secunde)");

    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        size_t clients = max_clients;
        int client_fds[max_clients];

        if (httpd_get_client_list(server, &clients, client_fds) == ESP_OK) {
            if (clients > 0) {
                // Generăm JSON-ul o singură dată pe ciclu, NU în interiorul loop-ului de clienți (economie masivă de CPU)
                static char json_response[(1024 * 2)]; // 2KB sunt suficienți acum
                char *p = json_response;

                // Obținem timpul Unix actual utilizând funcția ta dedicată
                time_t now = time_manager_get_unix_time();

                *p++ = '{';

                // Tritem Timestamp-ul Unix direct ca număr (mult mai curat pentru JS)
                p += sprintf(p, "\"epoch\":%ld,", (long)now);

                // DHT 1 Readings (Nume chei aliniate cu interfața)
                if (dht_valid_reading) {
                    p += sprintf(p, "\"temperature\":\"%.1f\",", dht_temp);
                    p += sprintf(p, "\"humidity\":\"%.1f\",", dht_humid);
                }
                else {
                    p += sprintf(p, "\"temperature\":\"--.-\",\"humidity\":\"--\",");
                }

                // DS Temp (Temperatura apei)
                if (ds_valid_reding) {
                    p += sprintf(p, "\"ds_temp\":\"%.1f\",", ds_temp);
                }
                else {
                    p += sprintf(p, "\"ds_temp\":\"--.-\",");
                }

                // Stare Pompe (0 sau 1)
                p += sprintf(p, "\"pompa_apa\":%u,", hydro_is_water_pump_on() ? 1 : 0);
                p += sprintf(p, "\"pompa_aer\":%u,", hydro_is_air_pump_on() ? 1 : 0);

                bool tank_is_low = citeste_senzor_nivel();
                // Dacă logica este inversată din pagină, schimbăm valoarea trimisă
                if (alarm_settings.float_switch_inv) {
                    tank_is_low = !tank_is_low;
                }
                p += sprintf(p, "\"tank_low\":%u,", tank_is_low ? 1 : 0);

                                // Diag Wifi și Sistem
                wifi_ap_record_t ap;
                if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
                    p += sprintf(p, "\"wifi_rssi\":%d,", ap.rssi);
                }
                else {
                    p += sprintf(p, "\"wifi_rssi\":0,");
                }

                uint32_t freeRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
                p += sprintf(p, "\"free_heap\":\"%u KB\",", (unsigned int)(freeRAM / 1024));

                char up_time[64];
                upTime(up_time);
                p += sprintf(p, "\"up_time\":\"%s\"", up_time); // Ultimul element nu are virgulă!

                *p++ = '}';
                *p++ = 0;

                // Trimitem pachetul către toți clienții conectați
                for (size_t i = 0; i < clients; ++i) {
                    int sock = client_fds[i];
                    if (httpd_ws_get_fd_info(server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
                        struct ws_async_resp_arg *resp_arg = calloc(1, sizeof(struct ws_async_resp_arg));
                        if (resp_arg == NULL) {
                            ESP_LOGE(TAG, "Calloc resp_arg fail");
                            break;
                        }

                        resp_arg->data = calloc(1, strlen(json_response) + 1);
                        if (resp_arg->data == NULL) {
                            ESP_LOGE(TAG, "Calloc resp_arg data fail");
                            free(resp_arg);
                            break;
                        }

                        memcpy(resp_arg->data, json_response, strlen(json_response));
                        resp_arg->len = strlen(json_response);
                        resp_arg->hd = server;
                        resp_arg->fd = sock;

                        if (httpd_queue_work(resp_arg->hd, ws_send_async, resp_arg) != ESP_OK) {
                            ESP_LOGE(TAG, "httpd_queue_work failed!");
                            free(resp_arg->data);
                            free(resp_arg);
                            break;
                        }
                    }
                }
            }
        }
        else {
            ESP_LOGE(TAG, "httpd_get_client_list failed!");
        }

        // Rulam task-ul fix o dată la 5 secunde (5000 ms)
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000));
    }
}

/*################################################
  ### START TASKS
  ################################################*/
esp_err_t start_server_task(void) {
    xTaskCreate(web_server_task, "web_data", 1024 * 8, NULL, 2, &WEB_SERVER_TaskHandle);
    if (WEB_SERVER_TaskHandle == NULL) {
        ESP_LOGE(TAG, "Fail: WEB_SERVER_TaskHandle");
        return ESP_FAIL;
    }

    return ESP_OK;
}
esp_err_t start_status_task(void) {
    xTaskCreate(web_status_task, "web_status", 1024 * 8, NULL, 2, &WEB_STATUS_TaskHandle);
    if (WEB_STATUS_TaskHandle == NULL) {
        ESP_LOGE(TAG, "Fail: WEB_STATUS_TaskHandle");
        return ESP_FAIL;
    }

    return ESP_OK;
}
