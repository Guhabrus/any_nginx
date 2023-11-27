#!/bin/bash

docker cp /home/makism/Myfolder/Scince/Docker_ngx/nginx_proxy/nginx_config/nginx.conf c2c15a4e2dde:/etc/nginx/nginx.conf


docker exec -it c2c15a4e2dde bash -c "nginx -s reload"