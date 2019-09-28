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

#include <oauth2/mem.h>

#include "cfg_int.h"
#include "openidc_int.h"

static bool
_oauth2_openidc_provider_metadata_parse(oauth2_log_t *log, const char *s_json,
					oauth2_openidc_provider_t **provider)
{
	bool rc = false;
	json_t *json = NULL;
	oauth2_openidc_provider_t *p = NULL;
	char *rv = NULL, *token_endpoint_auth = NULL;

	oauth2_debug(log, "enter");

	if (oauth2_json_decode_object(log, s_json, &json) == false) {
		oauth2_error(log, "could not parse json object");
		goto end;
	}

	*provider = oauth2_openidc_provider_init(log);
	p = *provider;
	if (p == NULL)
		goto end;

	if (oauth2_json_string_get(log, json, "issuer", &p->issuer, NULL) ==
	    false) {
		oauth2_error(log, "could not parse issuer");
		goto end;
	}
	if (oauth2_json_string_get(log, json, "authorization_endpoint",
				   &p->authorization_endpoint, NULL) == false) {
		oauth2_error(log, "could not parse authorization_endpoint");
		goto end;
	}
	if (oauth2_json_string_get(log, json, "token_endpoint",
				   &p->token_endpoint, NULL) == false) {
		oauth2_error(log, "could not parse token_endpoint");
		goto end;
	}
	if (oauth2_json_string_get(log, json, "jwks_uri", &p->jwks_uri, NULL) ==
	    false) {
		oauth2_error(log, "could not parse jwks_uri");
		goto end;
	}

	p->ssl_verify = json_is_true(json_object_get(json, "ssl_verify"));

	if (oauth2_json_string_get(log, json, "token_endpoint_auth",
				   &token_endpoint_auth,
				   "client_secret_basic") == false) {
		oauth2_error(log, "could not parse token_endpoint_auth");
		goto end;
	}

	// TODO: client file?

	if (oauth2_json_string_get(log, json, "client_id", &p->client_id,
				   NULL) == false) {
		oauth2_error(log, "could not parse client_id");
		goto end;
	}
	if (oauth2_json_string_get(log, json, "client_secret",
				   &p->client_secret, NULL) == false) {
		oauth2_error(log, "could not parse client_secret");
		goto end;
	}
	if (oauth2_json_string_get(log, json, "scope", &p->scope, "openid") ==
	    false) {
		oauth2_error(log, "could not parse scope");
		goto end;
	}

	// TODO:
	oauth2_nv_list_t *params = oauth2_nv_list_init(log);
	oauth2_nv_list_set(log, params, "client_id", p->client_id);
	oauth2_nv_list_set(log, params, "client_secret", p->client_secret);
	p->token_endpoint_auth = oauth2_cfg_endpoint_auth_init(log);
	rv = oauth2_cfg_endpoint_auth_add_options(log, p->token_endpoint_auth,
						  token_endpoint_auth, params);
	oauth2_nv_list_free(log, params);
	if (rv != NULL)
		goto end;

	rc = true;

end:

	if (token_endpoint_auth)
		oauth2_mem_free(token_endpoint_auth);
	if (json)
		json_decref(json);

	oauth2_debug(log, "leave: %d", rc);

	return rc;
}

#define OAUTH_OPENIDC_PROVIDER_CACHE_EXPIRY_DEFAULT 60 * 60 * 24

bool _oauth2_openidc_provider_resolve(oauth2_log_t *log,
				      const oauth2_cfg_openidc_t *cfg,
				      const oauth2_http_request_t *request,
				      const char *issuer,
				      oauth2_openidc_provider_t **provider)
{
	bool rc = false;
	char *s_json = NULL;

	if ((cfg->provider_resolver == NULL) ||
	    (cfg->provider_resolver->callback == NULL)) {
		oauth2_error(
		    log,
		    "configuration error: provider_resolver is not configured");
		goto end;
	}

	if (issuer) {

		oauth2_cache_get(log, cfg->provider_resolver->cache->cache,
				 issuer, &s_json);
	}

	if (s_json == NULL) {

		if (cfg->provider_resolver->callback(log, cfg, request,
						     &s_json) == false) {
			oauth2_error(log, "resolver callback returned false");
			goto end;
		}

		if (s_json == NULL) {
			oauth2_error(
			    log, "no provider was returned by the provider "
				 "resolver; probably a configuration error");
			goto end;
		}
	}

	if (_oauth2_openidc_provider_metadata_parse(log, s_json, provider) ==
	    false)
		goto end;

	// TODO: cache expiry configuration option
	oauth2_cache_set(log, cfg->provider_resolver->cache->cache,
			 oauth2_openidc_provider_issuer_get(log, *provider),
			 s_json, OAUTH_OPENIDC_PROVIDER_CACHE_EXPIRY_DEFAULT);

	rc = true;

end:

	if (s_json)
		oauth2_mem_free(s_json);

	return rc;
}

_OAUTH2_CFG_CTX_TYPE_SINGLE_STRING(oauth2_openidc_provider_resolver_file_ctx,
				   filename)

#define OAUTH2_OPENIDC_PROVIDER_RESOLVE_FILENAME_DEFAULT "conf/provider.json"

static bool _oauth2_openidc_provider_resolve_file(
    oauth2_log_t *log, const oauth2_cfg_openidc_t *cfg,
    const oauth2_http_request_t *request, char **s_json)
{
	bool rc = false;
	oauth2_openidc_provider_resolver_file_ctx_t *ctx = NULL;
	char *filename = NULL;

	oauth2_debug(log, "enter");

	ctx = (oauth2_openidc_provider_resolver_file_ctx_t *)
		  cfg->provider_resolver->ctx->ptr;
	filename = ctx->filename
		       ? ctx->filename
		       : OAUTH2_OPENIDC_PROVIDER_RESOLVE_FILENAME_DEFAULT;

	*s_json = oauth_read_file(log, filename);
	if (*s_json == NULL)
		goto end;

	rc = true;

end:

	oauth2_debug(log, "leave: %d", rc);

	return rc;
}

// TODO: must explicitly (re-)populate cache on startup!
#define OAUTH2_CFG_OPENIDC_PROVIDER_CACHE_DEFAULT 60 * 60 * 24

static char *_oauth2_cfg_openidc_provider_resolver_file_set_options(
    oauth2_log_t *log, const char *value, const oauth2_nv_list_t *params,
    void *c)
{
	oauth2_cfg_openidc_t *cfg = (oauth2_cfg_openidc_t *)c;

	// TODO: macroize?
	cfg->provider_resolver = oauth2_cfg_openidc_provider_resolver_init(log);
	cfg->provider_resolver->callback =
	    _oauth2_openidc_provider_resolve_file;
	cfg->provider_resolver->ctx->callbacks =
	    &oauth2_openidc_provider_resolver_file_ctx_funcs;
	cfg->provider_resolver->ctx->ptr =
	    cfg->provider_resolver->ctx->callbacks->init(log);

	// TODO: factor out?
	oauth2_openidc_provider_resolver_file_ctx_t *ctx =
	    (oauth2_openidc_provider_resolver_file_ctx_t *)
		cfg->provider_resolver->ctx->ptr;
	ctx->filename = oauth2_strdup(value);

	oauth2_cfg_cache_set_options(log, cfg->provider_resolver->cache,
				     "resolver", params,
				     OAUTH2_CFG_OPENIDC_PROVIDER_CACHE_DEFAULT);

	return NULL;
}

// DIR

static char *_oauth2_cfg_openidc_provider_resolver_dir_set_options(
    oauth2_log_t *log, const char *value, const oauth2_nv_list_t *params,
    void *c)
{

	// oauth2_cfg_cache_set_options(
	//   log, resolver->cache, "resolver",
	//  params, OAUTH2_CFG_OPENIDC_PROVIDER_CACHE_DEFAULT);

	return NULL;
}

// STRING

_OAUTH2_CFG_CTX_TYPE_SINGLE_STRING(oauth2_openidc_provider_resolver_str_ctx,
				   metadata)

static bool _oauth2_openidc_provider_resolve_string(
    oauth2_log_t *log, const oauth2_cfg_openidc_t *cfg,
    const oauth2_http_request_t *request, char **s_json)
{
	bool rc = false;
	oauth2_openidc_provider_resolver_str_ctx_t *ctx = NULL;

	oauth2_debug(log, "enter");

	ctx = (oauth2_openidc_provider_resolver_str_ctx_t *)
		  cfg->provider_resolver->ctx->ptr;
	if (ctx->metadata == NULL) {
		oauth2_error(log, "metadata not configured");
		goto end;
	}

	*s_json = oauth2_strdup(ctx->metadata);

	rc = true;

end:

	oauth2_debug(log, "leave: %d", rc);

	return rc;
}

char *_oauth2_cfg_openidc_provider_resolver_string_set_options(
    oauth2_log_t *log, const char *value, const oauth2_nv_list_t *params,
    void *c)
{
	oauth2_cfg_openidc_t *cfg = (oauth2_cfg_openidc_t *)c;
	oauth2_openidc_provider_resolver_str_ctx_t *ctx = NULL;

	cfg->provider_resolver = oauth2_cfg_openidc_provider_resolver_init(log);
	cfg->provider_resolver->callback =
	    _oauth2_openidc_provider_resolve_string;
	cfg->provider_resolver->ctx->callbacks =
	    &oauth2_openidc_provider_resolver_str_ctx_funcs;
	cfg->provider_resolver->ctx->ptr =
	    cfg->provider_resolver->ctx->callbacks->init(log);

	ctx = (oauth2_openidc_provider_resolver_str_ctx_t *)
		  cfg->provider_resolver->ctx->ptr;
	ctx->metadata = oauth2_strdup(value);

	return NULL;
}

#define OAUTH2_OPENIDC_PROVIDER_RESOLVER_STR_STR "string"
#define OAUTH2_OPENIDC_PROVIDER_RESOLVER_FILE_STR "file"
#define OAUTH2_OPENIDC_PROVIDER_RESOLVER_DIR_STR "dir"

// clang-format off
static oauth2_cfg_set_options_ctx_t _oauth2_cfg_resolver_options_set[] = {
	{ OAUTH2_OPENIDC_PROVIDER_RESOLVER_STR_STR, _oauth2_cfg_openidc_provider_resolver_string_set_options },
	{ OAUTH2_OPENIDC_PROVIDER_RESOLVER_FILE_STR, _oauth2_cfg_openidc_provider_resolver_file_set_options },
	{ OAUTH2_OPENIDC_PROVIDER_RESOLVER_DIR_STR, _oauth2_cfg_openidc_provider_resolver_dir_set_options },
	{ NULL, NULL }
};
// clang-format on

char *oauth2_cfg_openidc_provider_resolver_set_options(
    oauth2_log_t *log, oauth2_cfg_openidc_t *cfg, const char *type,
    const char *value, const char *options)
{
	return oauth2_cfg_set_options(log, cfg, type, value, options,
				      _oauth2_cfg_resolver_options_set);
}
