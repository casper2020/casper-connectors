/**
 * @file types.h
 *
 * Copyright (c) 2011-2023 Cloudware S.A. All rights reserved.
 *
 * This file is part of casper-connectors.
 *
 * casper-connectors is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * casper-connectors is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with casper-connectors. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef NGX_CASPER_CONFIG_MODULE_TYPES_H_
#define NGX_CASPER_CONFIG_MODULE_TYPES_H_

extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
    #include <ngx_errno.h>
    #include <ngx_conf_file.h>
}

/* BEANSTALKD data types */
typedef struct ngx_casper_beanstalk_tubes_conf_s {
    ngx_str_t                             action;
    ngx_str_t                             sessionless;
} ngx_casper_beanstalk_tubes_conf_t;

typedef struct ngx_casper_beanstalk_conf_s {
    ngx_str_t                              host;
    ngx_uint_t                             port;
    ngx_int_t                              timeout;
    ngx_casper_beanstalk_tubes_conf_t tubes;
} ngx_casper_beanstalk_conf_t;

/* REDIS data types */
typedef struct ngx_casper_redis_conf_s {
    ngx_str_t              ip_address;
    ngx_int_t              port_number;
    ngx_int_t              database;
    ngx_int_t              max_conn_per_worker;
} ngx_casper_redis_conf_t;

/* POSTGRESQL data types */
typedef struct ngx_casper_postgresql_conf_s {
    ngx_str_t                   conn_str;
    ngx_int_t                   statement_timeout;
    ngx_int_t                   max_conn_per_worker;
    ngx_int_t                   max_queries_per_conn;
    ngx_int_t                   min_queries_per_conn;
    ngx_str_t                   post_connect_queries;
} ngx_casper_postgresql_conf_t;

/* cURL data types*/
typedef struct ngx_casper_curl_conf_s {
    ngx_int_t max_conn_per_worker;
} ngx_casper_curl_conf_t;

/* GATEKEEPER data types */
typedef struct ngx_casper_gatekeeper_conf_s {
    ngx_str_t config_file_uri;
} ngx_casper_gatekeeper_conf_t;

#endif // NGX_CASPER_CONFIG_MODULE_TYPES_H_
