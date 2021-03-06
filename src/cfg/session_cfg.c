/***************************************************************************
 *
 * Copyright (C) 2018-2020 - ZmartZone Holding BV - www.zmartzone.eu
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

#include "oauth2/mem.h"

#include "cache_int.h"
#include "cfg_int.h"

typedef struct oauth2_cfg_session_list_t {
	char *name;
	oauth2_cfg_session_t *session;
	struct oauth2_cfg_session_list_t *next;
} oauth2_cfg_session_list_t;

static oauth2_cfg_session_list_t *_session_list = NULL;

#define OAUTH2_SESSION_TYPE_COOKIE_STR "cookie"
#define OAUTH2_SESSION_TYPE_CACHE_STR "cache"

static void _oauth2_cfg_session_register(oauth2_log_t *log, const char *name,
					 oauth2_cfg_session_t *session)
{
	oauth2_cfg_session_list_t *ptr = NULL, *prev = NULL;

	ptr = oauth2_mem_alloc(sizeof(oauth2_cfg_session_list_t));
	ptr->name = oauth2_strdup(name);
	ptr->session = session;
	ptr->next = NULL;
	if (_session_list) {
		prev = _session_list;
		while (prev->next)
			prev = prev->next;
		prev->next = ptr;
	} else {
		_session_list = ptr;
	}
}

oauth2_cfg_session_t *oauth2_cfg_session_init(oauth2_log_t *log)
{
	oauth2_cfg_session_t *session = NULL;
	session = (oauth2_cfg_session_t *)oauth2_mem_alloc(
	    sizeof(oauth2_cfg_session_t));
	session->type = OAUTH2_CFG_UINT_UNSET;
	session->cookie_name = NULL;
	session->inactivity_timeout_s = OAUTH2_CFG_TIME_UNSET;
	session->max_duration_s = OAUTH2_CFG_TIME_UNSET;

	session->passphrase = NULL;

	session->cache = NULL;

	session->load_callback = NULL;
	session->save_callback = NULL;

	return session;
}

oauth2_cfg_session_t *oauth2_cfg_session_clone(oauth2_log_t *log,
					       oauth2_cfg_session_t *src)
{
	oauth2_cfg_session_t *dst = NULL;

	if (src == NULL)
		goto end;

	dst = oauth2_cfg_session_init(log);
	dst->type = src->type;
	dst->cookie_name = oauth2_strdup(src->cookie_name);
	dst->inactivity_timeout_s = src->inactivity_timeout_s;
	dst->max_duration_s = src->max_duration_s;

	dst->passphrase = oauth2_strdup(src->passphrase);
	dst->cache = oauth2_cache_clone(log, src->cache);

end:
	return dst;
}

// void oauth2_cfg_session_merge(
//	    oauth2_log_t *log, oauth2_cfg_session_t *cfg,
//	    oauth2_cfg_session_t *add,
//	    oauth2_cfg_session_t *base) {
//}

void oauth2_cfg_session_free(oauth2_log_t *log, oauth2_cfg_session_t *session)
{
	if (session->cookie_name)
		oauth2_mem_free(session->cookie_name);
	if (session->passphrase)
		oauth2_mem_free(session->passphrase);
	if (session->cache)
		oauth2_cache_release(log, session->cache);
	oauth2_mem_free(session);
}

oauth2_cfg_session_t *_oauth2_cfg_session_obtain(oauth2_log_t *log,
						 const char *name)
{
	oauth2_cfg_session_list_t *ptr = NULL, *result = NULL;
	oauth2_cfg_session_t *cfg = NULL;

	if (_session_list == NULL) {
		cfg = oauth2_cfg_session_init(log);
		oauth2_cfg_session_set_options(
		    log, cfg, OAUTH2_SESSION_TYPE_CACHE_STR, NULL);
	}

	ptr = _session_list;
	while (ptr) {
		if (ptr->name) {
			if (strcmp(ptr->name, name) == 0) {
				result = ptr;
				break;
			}
		} else if ((name == NULL) || (strcmp("default", name) == 0)) {
			result = ptr;
		}
		ptr = ptr->next;
	}

	return result ? result->session : NULL;
}

void oauth2_cfg_session_release(oauth2_log_t *log,
				oauth2_cfg_session_t *session)
{

	oauth2_cfg_session_list_t *ptr = NULL, *prev = NULL;

	if (session)
		oauth2_cfg_session_free(log, session);

	ptr = _session_list;
	prev = NULL;
	while (ptr) {
		if (ptr->session == session) {
			if (prev)
				prev->next = ptr->next;
			else
				_session_list = ptr->next;
			if (ptr->name)
				oauth2_mem_free(ptr->name);
			oauth2_mem_free(ptr);
			break;
		}
		prev = ptr;
		ptr = ptr->next;
	}
}

#define OAUTH2_INACTIVITY_TIMEOUT_S_DEFAULT 60 * 5

oauth2_time_t
oauth2_cfg_session_inactivity_timeout_s_get(oauth2_log_t *log,
					    const oauth2_cfg_session_t *cfg)
{
	if ((cfg == NULL) ||
	    (cfg->inactivity_timeout_s == OAUTH2_CFG_TIME_UNSET))
		return OAUTH2_INACTIVITY_TIMEOUT_S_DEFAULT;
	return cfg->inactivity_timeout_s;
}

#define OAUTH2_SESSION_MAX_DURATION_S_DEFAULT 60 * 60 * 8

oauth2_time_t
oauth2_cfg_session_max_duration_s_get(oauth2_log_t *log,
				      const oauth2_cfg_session_t *cfg)
{
	if ((cfg == NULL) || (cfg->max_duration_s == OAUTH2_CFG_TIME_UNSET))
		return OAUTH2_SESSION_MAX_DURATION_S_DEFAULT;
	return cfg->max_duration_s;
}

#define OAUTH2_SESSION_COOKIE_NAME_DEFAULT "openidc_session"

char *oauth2_cfg_session_cookie_name_get(oauth2_log_t *log,
					 const oauth2_cfg_session_t *cfg)
{
	if ((cfg == NULL) || (cfg->cookie_name == NULL))
		return OAUTH2_SESSION_COOKIE_NAME_DEFAULT;
	return cfg->cookie_name;
}

// TODO: there most probably should NOT be a default for this setting
#define OAUTH2_SESSION_PASSPHRASE_DEFAULT "blabla1234"

char *oauth2_cfg_session_passphrase_get(oauth2_log_t *log,
					const oauth2_cfg_session_t *cfg)
{
	if ((cfg == NULL) || (cfg->passphrase == NULL))
		return OAUTH2_SESSION_PASSPHRASE_DEFAULT;
	return cfg->passphrase;
}

oauth2_session_load_callback_t *
oauth2_cfg_session_load_callback_get(oauth2_log_t *log,
				     const oauth2_cfg_session_t *cfg)
{
	if ((cfg == NULL) || (cfg->load_callback == NULL))
		return oauth2_session_load_cookie;
	return cfg->load_callback;
}

oauth2_session_save_callback_t *
oauth2_cfg_session_save_callback_get(oauth2_log_t *log,
				     const oauth2_cfg_session_t *cfg)
{
	if ((cfg == NULL) || (cfg->save_callback == NULL))
		return oauth2_session_save_cookie;
	return cfg->save_callback;
}

oauth2_cache_t *oauth2_cfg_session_cache_get(oauth2_log_t *log,
					     const oauth2_cfg_session_t *cfg)
{
	return cfg->cache;
}

_OAUTH_CFG_CTX_CALLBACK(oauth2_cfg_session_set_options_cookie)
{
	oauth2_cfg_session_t *cfg = (oauth2_cfg_session_t *)ctx;
	char *rv = NULL;

	oauth2_debug(log, "enter");

	cfg->type = OAUTH2_SESSION_TYPE_COOKIE;
	cfg->load_callback = oauth2_session_load_cookie;
	cfg->save_callback = oauth2_session_save_cookie;

	cfg->passphrase =
	    oauth2_strdup(oauth2_nv_list_get(log, params, "passphrase"));

	oauth2_debug(log, "leave: %s", rv);

	return rv;
}

#define OAUTH2_CFG_SESSION_CACHE_DEFAULT OAUTH2_SESSION_EXPIRY_S_DEFAULT

_OAUTH_CFG_CTX_CALLBACK(oauth2_cfg_session_set_options_cache)
{
	oauth2_cfg_session_t *cfg = (oauth2_cfg_session_t *)ctx;
	char *rv = NULL;

	oauth2_debug(log, "enter");

	cfg->type = OAUTH2_SESSION_TYPE_CACHE;
	cfg->load_callback = oauth2_session_load_cache;
	cfg->save_callback = oauth2_session_save_cache;

	cfg->cache =
	    _oauth2_cache_obtain(log, oauth2_nv_list_get(log, params, "cache"));

	oauth2_debug(log, "leave: %s", rv);

	return rv;
}

// clang-format off
static oauth2_cfg_set_options_ctx_t _oauth2_cfg_session_options_set[] = {
	{ OAUTH2_SESSION_TYPE_COOKIE_STR, oauth2_cfg_session_set_options_cookie },
	{ OAUTH2_SESSION_TYPE_CACHE_STR, oauth2_cfg_session_set_options_cache },
	{ NULL, NULL }
};
// clang-format on

char *oauth2_cfg_session_set_options(oauth2_log_t *log,
				     oauth2_cfg_session_t *cfg,
				     const char *type, const char *options)
{
	char *rv = NULL;
	oauth2_nv_list_t *params = NULL;
	const char *value = NULL;

	if (cfg == NULL) {
		rv = oauth2_strdup("internal error: cfg is null");
		goto end;
	}

	rv = oauth2_cfg_set_options(log, cfg, type, NULL, options,
				    _oauth2_cfg_session_options_set);
	if (rv != NULL)
		goto end;

	if (oauth2_parse_form_encoded_params(log, options, &params) == false)
		goto end;

	value = oauth2_nv_list_get(log, params, "cookie_name");
	if (value)
		cfg->cookie_name = oauth2_strdup(value);

	value = oauth2_nv_list_get(log, params, "max_duration");
	if (value)
		cfg->max_duration_s =
		    oauth2_parse_time_sec(log, value, OAUTH2_CFG_TIME_UNSET);

	value = oauth2_nv_list_get(log, params, "inactivity_timeout");
	if (value)
		cfg->inactivity_timeout_s =
		    oauth2_parse_time_sec(log, value, OAUTH2_CFG_TIME_UNSET);

	_oauth2_cfg_session_register(
	    log, oauth2_nv_list_get(log, params, "name"), cfg);

end:

	if (params)
		oauth2_nv_list_free(log, params);

	return rv;
}
