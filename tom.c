#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include <toml.h>

// ساختارهای داده
typedef struct {
    char* key;
    char* gav; // group:artifact:version
} Dependency;

typedef struct {
    char* project_name;
    char* version;
    char* output_type; // "jar", "fatjar"
    char* main_class;
    char* output_path;
    Dependency* dependencies;
    int dep_count;
} BuildConf;

// تابع اجرای دستور
void run(const char* cmd) {
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "Command failed: %s\n", cmd);
        exit(1);
    }
}

// تابع تجزیه GAV
void parse_gav(const char* gav, char* group, char* artifact, char* version) {
    char* dup = strdup(gav);
    char* tok = strtok(dup, ":");
    strcpy(group, tok);
    tok = strtok(NULL, ":");
    strcpy(artifact, tok);
    tok = strtok(NULL, ":");
    strcpy(version, tok);
    free(dup);
}

// تابع دانلود یک وابستگی
void downloadonedep(const char* gav) {
    char group[256], artifact[256], version[256];
    parse_gav(gav, group, artifact, version);

    // تبدیل group (.) به (/)
    char group_path[512];
    strcpy(group_path, group);
    for (int i = 0; group_path[i]; i++) {
        if (group_path[i] == '.') group_path[i] = '/';
    }

    char url[1024];
    snprintf(url, sizeof(url), "https://repo1.maven.org/maven2/%s/%s/%s/%s-%s.jar", 
             group_path, artifact, version, artifact, version);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -L -o libs/%s-%s.jar '%s'", artifact, version, url);
    run(cmd);

    // دانلود .pom
    snprintf(url, sizeof(url), "https://repo1.maven.org/maven2/%s/%s/%s/%s-%s.pom", 
             group_path, artifact, version, artifact, version);
    snprintf(cmd, sizeof(cmd), "curl -L -o libs/%s-%s.pom '%s'", artifact, version, url);
    run(cmd);
}

// تابع استخراج محتوای تگ از متن XML
char* extract_tag_content(const char* start, const char* tag_name) {
    char tag_start[64];
    char tag_end[64];
    snprintf(tag_start, sizeof(tag_start), "<%s>", tag_name);
    snprintf(tag_end, sizeof(tag_end), "</%s>", tag_name);

    const char* s = strstr(start, tag_start);
    if (!s) return NULL;
    s += strlen(tag_start);

    const char* e = strstr(s, tag_end);
    if (!e) return NULL;

    // ایجاد کپی و حذف کاراکترهای اضافی
    char* content = strndup(s, e - s);
    // حذف خطوط جدید و فاصله‌ها
    char* clean = malloc(strlen(content) + 1);
    char* dst = clean;
    for (char* src = content; *src; src++) {
        if (*src != '\n' && *src != '\t' && *src != ' ') {
            *dst++ = *src;
        }
    }
    *dst = '\0';
    free(content);
    return clean;
}

// تابع دانلود .pom و استخراج وابستگی‌های ضمنی (دقیق و با لاگ)
void parsepomandqueuedependencies(const char* gav) {
    char group[256], artifact[256], version[256];
    parse_gav(gav, group, artifact, version);

    char pom_path[1024];
    snprintf(pom_path, sizeof(pom_path), "libs/%s-%s.pom", artifact, version);

    FILE* fp = fopen(pom_path, "r");
    if (!fp) {
        printf("Warning: Could not open .pom file: %s\n", pom_path);
        return;
    }

    printf("Parsing POM for %s\n", gav);

    char* content = NULL;
    size_t len = 0;
    char buffer[4096];

    // خواندن کل فایل در یک رشته
    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t n = strlen(buffer);
        content = realloc(content, len + n + 1);
        memcpy(content + len, buffer, n);
        len += n;
    }
    content[len] = '\0';
    fclose(fp);

    // جستجو در محتوا
    char* pos = content;
    while ((pos = strstr(pos, "<dependency>"))) {
        pos += strlen("<dependency>");

        char* dep_group = extract_tag_content(pos, "groupId");
        char* dep_artifact = extract_tag_content(pos, "artifactId");
        char* dep_version = extract_tag_content(pos, "version");
        char* scope = extract_tag_content(pos, "scope");

        char* opt_tag = extract_tag_content(pos, "optional");
        int optional = 0;
        if (opt_tag && strcmp(opt_tag, "true") == 0) {
            optional = 1;
        }

        if (dep_group && dep_artifact && dep_version) {
            if (optional) {
                printf("Skipping dependency %s:%s:%s (optional=true)\n", dep_group, dep_artifact, dep_version);
            } else if (scope && strcmp(scope, "compile") != 0 && strcmp(scope, "runtime") != 0) {
                printf("Skipping dependency %s:%s:%s (scope=%s)\n", dep_group, dep_artifact, dep_version, scope);
            } else {
                char gav_new[1024];
                snprintf(gav_new, sizeof(gav_new), "%s:%s:%s", dep_group, dep_artifact, dep_version);
                printf("Found transitive dependency: %s (scope: %s)\n", gav_new, scope ? scope : "compile");
                downloadonedep(gav_new);
                parsepomandqueuedependencies(gav_new); // بازگشتی
            }
        }

        free(dep_group);
        free(dep_artifact);
        free(dep_version);
        free(scope);
        free(opt_tag);
    }

    free(content);
}

// دانلود تمام وابستگی‌ها (اصلی و وابستگی‌های داخلی)
void cmddownload(BuildConf* conf) {
    mkdir("libs", 0755);
    for (int i = 0; i < conf->dep_count; i++) {
        downloadonedep(conf->dependencies[i].gav);
        parsepomandqueuedependencies(conf->dependencies[i].gav);
    }
}

// نوشتن فایل MANIFEST.MF
void write_manifest(const char* path, const char* main_class) {
    FILE* f = fopen(path, "w");
    if (!f) {
        perror("Cannot write MANIFEST.MF");
        exit(1);
    }
    fprintf(f, "Manifest-Version: 1.0\n");
    if (main_class) {
        fprintf(f, "Main-Class: %s\n", main_class);
    }
    fclose(f);
}

// ساخت jar ساده
void cmd_jar(BuildConf* conf) {
    if (conf->main_class) {
        write_manifest("MANIFEST.MF", conf->main_class);
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "javac -cp 'libs/*' src/ping/Main.java -d out/");
    run(cmd);

    snprintf(cmd, sizeof(cmd), "jar cfm %s MANIFEST.MF -C out .", conf->output_path);
    run(cmd);
}

// ساخت fatjar
void cmd_fatjar(BuildConf* conf) {
    if (conf->main_class) {
        write_manifest("MANIFEST.MF", conf->main_class);
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "javac -cp 'libs/*' src/ping/Main.java -d out/");
    run(cmd);

    snprintf(cmd, sizeof(cmd), "jar uf %s -C out .", conf->output_path);
    run(cmd);

    // اکنون تمام .classهای وابستگی‌ها را به .jar اضافه می‌کنیم
    snprintf(cmd, sizeof(cmd), "for jar in libs/*.jar; do jar -xf \"$jar\"; done");
    run(cmd);

    snprintf(cmd, sizeof(cmd), "jar cfm %s MANIFEST.MF .", conf->output_path);
    run(cmd);
}

// ساخت بر اساس نوع خروجی
void cmd_make(BuildConf* conf) {
    if (strcmp(conf->output_type, "jar") == 0) {
        cmd_jar(conf);
    } else if (strcmp(conf->output_type, "fatjar") == 0) {
        cmd_fatjar(conf);
    } else {
        fprintf(stderr, "Unknown output_type: %s\n", conf->output_type);
        exit(1);
    }
}

// تابع اصلی ساخت (دانلود + ساخت)
void cmdbuild(BuildConf* conf) {
    cmddownload(conf);
    cmd_make(conf);
}

// خواندن فایل build.toml
BuildConf* load_config(const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) {
        perror("Cannot open build.toml");
        exit(1);
    }

    char errbuf[200];
    toml_table_t* conf_table = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!conf_table) {
        fprintf(stderr, "TOML parse error: %s\n", errbuf);
        exit(1);
    }

    BuildConf* build_conf = malloc(sizeof(BuildConf));

    toml_datum_t name_datum = toml_string_in(conf_table, "name");
    build_conf->project_name = name_datum.ok ? name_datum.u.s : strdup("unknown");

    toml_datum_t version_datum = toml_string_in(conf_table, "version");
    build_conf->version = version_datum.ok ? version_datum.u.s : strdup("0.0.0");

    toml_datum_t output_type_datum = toml_string_in(conf_table, "output_type");
    build_conf->output_type = output_type_datum.ok ? output_type_datum.u.s : strdup("jar");

    toml_datum_t main_class_datum = toml_string_in(conf_table, "main_class");
    build_conf->main_class = main_class_datum.ok ? main_class_datum.u.s : NULL;

    toml_datum_t output_path_datum = toml_string_in(conf_table, "output_path");
    build_conf->output_path = output_path_datum.ok ? output_path_datum.u.s : strdup("output.jar");

    toml_array_t* deps_arr = toml_array_in(conf_table, "dependencies");
    int n = deps_arr ? toml_array_nelem(deps_arr) : 0;
    build_conf->dep_count = n;
    build_conf->dependencies = malloc(n * sizeof(Dependency));

    for (int i = 0; i < n; i++) {
        toml_table_t* dep = toml_table_at(deps_arr, i);
        if (!dep) continue;

        toml_datum_t key_datum = toml_string_in(dep, "key");
        toml_datum_t gav_datum = toml_string_in(dep, "gav");

        build_conf->dependencies[i].key = key_datum.ok ? key_datum.u.s : NULL;
        build_conf->dependencies[i].gav = gav_datum.ok ? gav_datum.u.s : NULL;

        if (build_conf->dependencies[i].key) {
            build_conf->dependencies[i].key = strdup(build_conf->dependencies[i].key);
        }
        if (build_conf->dependencies[i].gav) {
            build_conf->dependencies[i].gav = strdup(build_conf->dependencies[i].gav);
        }
    }

    toml_free(conf_table);
    return build_conf;
}

// تابع اصلی
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [build|download|make]\n", argv[0]);
        return 1;
    }

    BuildConf* conf = load_config("build.toml");

    if (strcmp(argv[1], "build") == 0) {
        cmdbuild(conf);
    } else if (strcmp(argv[1], "download") == 0) {
        cmddownload(conf);
    } else if (strcmp(argv[1], "make") == 0) {
        cmd_make(conf);
    }

    // آزادسازی حافظه
    for (int i = 0; i < conf->dep_count; i++) {
        free(conf->dependencies[i].key);
        free(conf->dependencies[i].gav);
    }
    free(conf->dependencies);
    free(conf->project_name);
    free(conf->version);
    free(conf->output_type);
    if (conf->main_class) free(conf->main_class);
    free(conf->output_path);
    free(conf);

    return 0;
}
