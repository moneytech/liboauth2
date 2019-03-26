#ifndef _OAUTH2_CFG_INT_H_
#define _OAUTH2_CFG_INT_H_

/***************************************************************************
 *
 * Copyright (C) 2018-2019 - ZmartZone Holding BV - www.zmartzone.eu
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @Author: Hans Zandbelt - hans.zandbelt@zmartzone.eu
 *
 **************************************************************************/

#include "oauth2/cache.h"
#include "oauth2/cfg.h"

#include <cjose/cjose.h>

/*
 * auth
 */

typedef struct oauth2_cfg_endpoint_auth_none_t {
	uint8_t dummy;
} oauth2_cfg_endpoint_auth_none_t;

typedef struct oauth2_cfg_endpoint_auth_client_secret_basic_t {
	char *client_id;
	char *client_secret;
} oauth2_cfg_endpoint_auth_client_secret_basic_t;

typedef struct oauth2_cfg_endpoint_auth_client_secret_post_t {
	char *client_id;
	char *client_secret;
} oauth2_cfg_endpoint_auth_client_secret_post_t;

typedef struct oauth2_cfg_endpoint_auth_client_secret_jwt_t {
	char *client_id;
	cjose_jwk_t *jwk;
	char *aud;
} oauth2_cfg_endpoint_auth_client_secret_jwt_t;

typedef struct oauth2_cfg_endpoint_auth_private_key_jwt_t {
	char *client_id;
	cjose_jwk_t *jwk;
	char *aud;
} oauth2_cfg_endpoint_auth_private_key_jwt_t;

typedef struct oauth2_cfg_endpoint_auth_client_cert_t {
	char *certfile;
	char *keyfile;
} oauth2_cfg_endpoint_auth_client_cert_t;

typedef struct oauth2_cfg_endpoint_auth_basic_t {
	char *username;
	char *password;
} oauth2_cfg_endpoint_auth_basic_t;

typedef struct oauth2_cfg_endpoint_auth_t {
	oauth2_cfg_endpoint_auth_type_t type;
	union {
		oauth2_cfg_endpoint_auth_none_t none;
		oauth2_cfg_endpoint_auth_client_secret_basic_t
		    client_secret_basic;
		oauth2_cfg_endpoint_auth_client_secret_post_t
		    client_secret_post;
		oauth2_cfg_endpoint_auth_client_secret_jwt_t client_secret_jwt;
		oauth2_cfg_endpoint_auth_private_key_jwt_t private_key_jwt;
		oauth2_cfg_endpoint_auth_client_cert_t client_cert;
		oauth2_cfg_endpoint_auth_basic_t basic;
	};
} oauth2_cfg_endpoint_auth_t;

/*
 * source token
 */

typedef struct oauth2_cfg_source_token_t {
	oauth2_cfg_token_in_t accept_in;
	oauth2_flag_t strip;
} oauth2_cfg_source_token_t;

/*
 * cache
 */

typedef struct oauth2_cfg_cache_t {
	oauth2_cache_t *cache;
	oauth2_time_t expiry_s;
} oauth2_cfg_cache_t;

oauth2_cfg_cache_t *oauth2_cfg_cache_init(oauth2_log_t *log);
oauth2_cfg_cache_t *oauth2_cfg_cache_clone(oauth2_log_t *log,
					   oauth2_cfg_cache_t *src);
void oauth2_cfg_cache_free(oauth2_log_t *log, oauth2_cfg_cache_t *cache);

char *oauth2_cfg_cache_set_options(oauth2_log_t *log, oauth2_cfg_cache_t *cache,
				   const char *prefix,
				   const oauth2_nv_list_t *params,
				   oauth2_uint_t default_expiry_s);

/*
 * verify
 */

typedef void *(oauth2_cfg_ctx_init_cb)(oauth2_log_t *log);
typedef void *(oauth2_cfg_ctx_clone_cb)(oauth2_log_t *log, void *src);
typedef void(oauth2_cfg_ctx_free_cb)(oauth2_log_t *log, void *);

typedef struct oauth2_cfg_ctx_funcs_t {
	oauth2_cfg_ctx_init_cb *init;
	oauth2_cfg_ctx_clone_cb *clone;
	oauth2_cfg_ctx_free_cb *free;
} oauth2_cfg_ctx_funcs_t;

typedef bool(oauth2_cfg_token_verify_cb_t)(oauth2_log_t *,
					   oauth2_cfg_token_verify_t *,
					   const char *, json_t **,
					   char **s_payload);

typedef struct oauth2_cfg_ctx_t {
	void *ptr;
	oauth2_cfg_ctx_funcs_t *callbacks;
} oauth2_cfg_ctx_t;

typedef struct oauth2_cfg_token_verify_t {
	oauth2_cfg_token_verify_cb_t *callback;
	oauth2_cfg_ctx_t *ctx;
	oauth2_cfg_cache_t *cache;
	struct oauth2_cfg_token_verify_t *next;
} oauth2_cfg_token_verify_t;

#endif /* _OAUTH2_CFG_INT_H_ */