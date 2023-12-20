static ngx_int_t ngx_shared_list_remove_node(ngx_shm_zone_t *shm_zone, ngx_str_t name);

// ...

static ngx_shared_list_node_t *ngx_shared_list_remove_node(ngx_shm_zone_t *shm_zone, ngx_str_t name) {
    ngx_shared_list_node_t *list = (ngx_shared_list_node_t *)shm_zone->data;
    ngx_uint_t current = 0;
    ngx_uint_t prev = NGX_CONF_UNSET_UINT;

    // Поиск узла в списке
    while (current != NGX_CONF_UNSET_UINT) {
        if (ngx_strncmp(list[current].name.data, name.data, name.len) == 0) {
            // Узел найден, удаляем его из списка
            if (prev != NGX_CONF_UNSET_UINT) {
                // Если не первый элемент списка, обновляем указатель предыдущего узла
                list[prev].next = list[current].next;
            } else {
                // Если первый элемент списка, обновляем начало списка
                ngx_uint_t next = list[current].next;
                ngx_memset(&list[current], 0, sizeof(ngx_shared_list_node_t));
                list[0].next = next;
            }

            // Очищаем узел
            ngx_memset(&list[current], 0, sizeof(ngx_shared_list_node_t));

            return &list[current];
        }

        prev = current;
        current = list[current].next;
    }

    // Узел не найден
    return NULL;
}
