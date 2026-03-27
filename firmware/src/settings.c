/**
 * @file settings.c
 *
 * @brief
 *
 * @date 3/27/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <settings.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

#if defined(CONFIG_SETTINGS_FILE)
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#endif

#define STORAGE_PARTITION_ID FIXED_PARTITION_ID(storage_partition)

struct ares_settings_dict {
    const char *key;
    int32_t value;
};

#define GENERATE_ARES_DEFAULTS(name_, value_) value_,

static const int32_t defaults[] = {
    FOREACH_ARES_SETTING(GENERATE_ARES_DEFAULTS)};

#undef GENERATE_ARES_DEFAULTS

#define GENERATE_ARES_SETTINGS(name_, value_) {#name_, value_},

static struct ares_settings_dict settings[] = {
    FOREACH_ARES_SETTING(GENERATE_ARES_SETTINGS)};

#undef GENERATE_ARES_SETTINGS

#define MAX_NAME_LENGTH SETTINGS_MAX_NAME_LEN

static int handle_get(const char *name, char *val, int val_len_max) {
    ARG_UNUSED(val_len_max);
    const char *next;

    for (size_t i = 0; i < ARRAY_SIZE(settings); i++) {
        if (settings_name_steq(name, settings[i].key, &next) && !next) {
            memcpy(val, &settings[i].value, sizeof(settings[0].value));
            return sizeof(settings[0].value);
        }
    }

    return -ENOENT;
}

static int handle_set(const char *name, size_t len, settings_read_cb read_cb,
                      void *cb_arg) {
    ARG_UNUSED(len);
    const char *next;
    size_t name_len;
    int rc = -ENOENT;

    name_len = settings_name_next(name, &next);

    for (size_t i = 0; (i < ARRAY_SIZE(settings)) && !next; i++) {
        if (strncmp(name, settings[i].key, name_len) == 0) {
            rc = read_cb(cb_arg, &settings[i].value, sizeof(settings[0].value));
            break;
        }
    }

    return rc;
}

static int handle_commit(void) { return 0; }

static int handle_export(int (*cb)(const char *name, const void *value,
                                   size_t val_len)) {
    char name[MAX_NAME_LENGTH + 1];

    for (size_t i = 0; i < ARRAY_SIZE(settings); i++) {
        snprintf(name, MAX_NAME_LENGTH, "ares/'%s", settings[i].key);
        (void)cb(name, &settings[i].value, sizeof(settings[0].value));
    }

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(ares_settings, "ares", handle_get, handle_set,
                               handle_commit, handle_export);

int update_setting(enum ares_setting setting, int32_t value) {
    char name[MAX_NAME_LENGTH + 1];
    int rc;

    if (setting >= ARES_SETTING_RESERVED) {
        return -EINVAL;
    }

    snprintf(name, MAX_NAME_LENGTH, "ares/%s", settings[setting].key);

    rc = settings_save_one(name, &value, sizeof(value));

    if (rc == 0) {
        settings[setting].value = value;
    }

    return 0;
}

int retrieve_setting(enum ares_setting setting, int32_t *value) {
    if (setting >= ARES_SETTING_RESERVED || value == NULL) {
        return -EINVAL;
    }

    *value = settings[setting].value;
    return 0;
}

void reset_settings(void) {
    for (size_t i = 0; i < ARRAY_SIZE(settings); i++) {
        settings[i].value = defaults[i];
    }

    settings_save();
}

static int init_settings(void) {
    int rc;

    __ASSERT_NO_MSG(ARRAY_SIZE(defaults) == ARRAY_SIZE(settings));

#if defined(CONFIG_SETTINGS_FILE)
    FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(cstorage);

    static struct fs_mount littlefs_mnt = {.type = FS_LITTLEFS,
                                           .fs_data = &cstorage,
                                           .storage_dev =
                                               (void *)STORAGE_PARTITION_ID,
                                           .mnt_point = "/ff"};

    rc = fs_mount(&littlefs_mnt);
    if (rc != 0) {
        return rc;
    }

    rc = fs_unlink(CONFIG_SETTINGS_FILE_PATH);
    if ((rc != 0) && (rc != -ENOENT)) {
        return rc;
    }
#endif

    rc = settings_subsys_init();

    if (rc != 0) {
        return rc;
    }

    return settings_load();
}
SYS_INIT(init_settings, APPLICATION, 10);
