#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_str_t   name;
    ngx_uint_t  value;
    ngx_uint_t  next;  // Индекс следующего элемента в списке
} ngx_shared_list_node_t;

typedef struct {
    ngx_shm_zone_t  *shm_zone;
} ngx_shared_list_conf_t;

static ngx_int_t ngx_shared_list_init(ngx_conf_t *cf);
static ngx_int_t ngx_shared_list_handler(ngx_http_request_t *r);
static ngx_int_t ngx_shared_list_add_node(ngx_shm_zone_t *shm_zone, ngx_str_t name, ngx_uint_t value);
static ngx_shared_list_node_t *ngx_shared_list_find_node(ngx_shm_zone_t *shm_zone, ngx_str_t name);

static ngx_command_t ngx_shared_list_commands[] = {
    {
        ngx_string("shared_list"),
        NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
        ngx_shared_list_init,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_shared_list_module_ctx = {
    NULL,                          /* preconfiguration */
    ngx_shared_list_init,          /* postconfiguration */
    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */
    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */
    NULL,                          /* create location configuration */
    NULL                           /* merge location configuration */
};

ngx_module_t ngx_http_shared_list_module = {
    NGX_MODULE_V1,
    &ngx_shared_list_module_ctx,   /* module context */
    ngx_shared_list_commands,      /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_shared_list_init(ngx_conf_t *cf) {
    ngx_http_core_main_conf_t  *cmcf;
    ngx_shm_zone_t            *shm_zone;
    ngx_str_t                 *zone_name = ngx_palloc(cf->pool, sizeof(ngx_str_t));

    // Имя общей памяти
    *zone_name = ngx_string("shared_list_zone");

    // Регистрация общей памяти
    shm_zone = ngx_shared_memory_add(cf, zone_name, 2 * sizeof(ngx_shared_list_node_t), &ngx_http_shared_list_module);
    if (shm_zone == NULL) {
        return NGX_ERROR;
    }

    shm_zone->init = NULL;

    // Установка данных общей памяти
    ngx_shared_list_node_t *list = (ngx_shared_list_node_t *)shm_zone->data;
    list[0].next = 1;  // Начальный элемент списка
    list[1].next = NGX_CONF_UNSET_UINT;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    ngx_http_handler_pt *h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_shared_list_handler;

    return NGX_OK;
}

static ngx_int_t ngx_shared_list_handler(ngx_http_request_t *r) {
    ngx_int_t                 rc;
    ngx_http_complex_value_t  cv;
    ngx_str_t                 value;
    ngx_shared_list_conf_t   *slcf;
    ngx_shared_list_node_t   *node;
    ngx_shm_zone_t           *shm_zone;

    // Получение конфигурации модуля
    slcf = ngx_http_get_module_loc_conf(r, ngx_http_shared_list_module);

    // Получение зоны общей памяти
    shm_zone = slcf->shm_zone;

    // Инициализация значения для добавления в список
    ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));
    ngx_str_set(&cv.value, "NewValue");

    if (ngx_http_complex_value(r, &cv, &value) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // Добавление узла в общую память
    rc = ngx_shared_list_add_node(shm_zone, value, 42);
    if (rc != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // Поиск узла в общей памяти
    node = ngx_shared_list_find_node(shm_zone, value);
    if (node == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // Вывод данных узла
    ngx_buf_t   *b;
    ngx_chain_t  out;

    r->headers_out.content_type.len = sizeof("text/plain") - 1;
    r->headers_out.content_type.data = (u_char *) "text/plain";

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    b->pos = (u_char *)node->name.data;
    b->last = b->pos + node->name.len;
    b->memory = 1;
    b->last_buf = 1;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}

static ngx_shared_list_node_t *ngx_shared_list_find_node(ngx_shm_zone_t *shm_zone, ngx_str_t name) {
    ngx_shared_list_node_t *list = (ngx_shared_list_node_t *)shm_zone->data;
    ngx_uint_t current = 0;

    while (current != NGX_CONF_UNSET_UINT) {
        if (ngx_strncmp(list[current].name.data, name.data, name.len) == 0) {
            return &list[current];
        }

        current = list[current].next;
    }

    return NULL;
}

static ngx_int_t ngx_shared_list_add_node(ngx_shm_zone_t *shm_zone, ngx_str_t name, ngx_uint_t value) {
    ngx_shared_list_node_t *list = (ngx_shared_list_node_t *)shm_zone->data;
    ngx_uint_t current = 0;

    // Поиск свободного узла в списке
    while (list[current].next != NGX_CONF_UNSET_UINT) {
        current = list[current].next;
    }

    // Добавление нового узла
    ngx_memcpy(list[current].name.data, name.data, name.len);
    list[current].name.len = name.len;
    list[current].value = value;
    list[current].next = NGX_CONF_UNSET_UINT;

    return NGX_OK;
}
