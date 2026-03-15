#include "../lib/cJSON.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Memory {
  char *response;
  size_t size;
};

struct config_t {
  char *latitude;
  char *longitude;
} config;

static size_t write_callback(void *data, size_t size, size_t nmemb,
                             void *userp) {
  size_t realsize = size * nmemb;
  struct Memory *mem = (struct Memory *)userp;

  char *ptr = realloc(mem->response, mem->size + realsize + 1);
  if (!ptr)
    return 0;

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;

  return realsize;
}

#define MAX_COORD_LEN 8

int api_geo() {
  const char *url = "https://addr.se/geo";

  CURL *curl;
  struct Memory chunk = {0};

  CURLcode result = curl_global_init(CURL_GLOBAL_ALL);
  if (result) {
    fprintf(stderr, "curl_global_init() failed: %s\n",
            curl_easy_strerror(result));
    return 1;
  }

  curl = curl_easy_init();
  if (!curl) {
    curl_global_cleanup();
    return 1;
  }

  curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/8.18.0");
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

  result = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  curl_global_cleanup();

  if (result != CURLE_OK) {
    fprintf(stderr, "Request failed: %s\n", curl_easy_strerror(result));
    free(chunk.response);
    return 1;
  }

  char *latitude = NULL;
  char *longitude = NULL;
  char *line = chunk.response;
  int lineno = 0;

  while (line && *line) {
    char *next = strchr(line, '\n');
    if (next)
      *next = '\0';

    lineno++;
    if (lineno == 9) {
      char *val = strchr(line, ':');
      if (val)
        latitude = strdup(val + 2);
    } else if (lineno == 10) {
      char *val = strchr(line, ':');
      if (val)
        longitude = strdup(val + 2);
    }

    if (next)
      line = next + 1;
    else
      break;
  }

  free(chunk.response);

  if (!latitude || !longitude) {
    fprintf(stderr, "Failed to parse coordinates from response\n");
    free(latitude);
    free(longitude);
    return 1;
  }

  char *home = getenv("HOME");
  if (!home) {
    fprintf(stderr, "HOME not set\n");
    free(latitude);
    free(longitude);
    return 1;
  }

  char path[512];
  snprintf(path, sizeof(path), "%s/.clotemp", home);

  FILE *f = fopen(path, "w");
  if (!f) {
    fprintf(stderr, "Failed to write %s\n", path);
    free(latitude);
    free(longitude);
    return 1;
  }

  fprintf(f, "latitude: %s\nlongitude: %s\n", latitude, longitude);
  fclose(f);

  printf("Wrote %s\n", path);

  free(latitude);
  free(longitude);
  return 0;
}

char *api_request(char *latitude, char *longitude) {

  if (strlen(latitude) > MAX_COORD_LEN || strlen(longitude) > MAX_COORD_LEN) {
    fprintf(stderr, "Coordinates exceed max length of %d characters\n",
            MAX_COORD_LEN);
    return NULL;
  }

  const char *url_fmt = "https://api.open-meteo.com/v1/"
                        "forecast?latitude=%s&longitude=%s&current="
                        "temperature_2m&timezone=auto";
  const int url_len = snprintf(NULL, 0, url_fmt, latitude, longitude);
  char *url = malloc(url_len + 1);
  sprintf(url, url_fmt, latitude, longitude);

  CURL *curl;
  struct Memory chunk = {0};

  CURLcode result = curl_global_init(CURL_GLOBAL_ALL);

  if (result)
    fprintf(stderr, "curl_global_init() failed: %s\n",
            curl_easy_strerror(result));

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    result = curl_easy_perform(curl);

    if (result == CURLE_OK) {
      cJSON *json = cJSON_Parse(chunk.response);

      if (json) {
        cJSON *current = cJSON_GetObjectItem(json, "current");
        if (current) {
          cJSON *time = cJSON_GetObjectItem(current, "time");
          cJSON *temperature = cJSON_GetObjectItem(current, "temperature_2m");
          if (temperature && time) {
            const char *fmt = "%s // %.1f°C";
            int len = snprintf(NULL, 0, fmt, time->valuestring,
                               temperature->valuedouble);
            char *chunk_response = malloc(len + 1);
            if (chunk_response) {
              sprintf(chunk_response, fmt, time->valuestring,
                      temperature->valuedouble);
              return chunk_response;
            }
          }
        }
      }
      free(url);
      free(chunk.response);
      cJSON_Delete(json);
      return NULL;
    }
  }
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return NULL;
}

struct config_t *read_config() {
  char *HOME = getenv("HOME");
  if (!HOME)
    return NULL;

  char config_path[512];
  snprintf(config_path, sizeof(config_path), "%s/.clotemp", HOME);

  FILE *textfile = fopen(config_path, "r");
  if (textfile == NULL)
    return NULL;

  char line[256];
  char lat_buf[32];
  char lon_buf[32];
  int items_read = 0;

  while (fgets(line, sizeof(line), textfile)) {
    if (sscanf(line, "latitude: %s", lat_buf) == 1) {
      config.latitude = strdup(lat_buf);
      items_read++;
    } else if (sscanf(line, "longitude: %s", lon_buf) == 1) {
      config.longitude = strdup(lon_buf);
      items_read++;
    }
  }

  fclose(textfile);

  if (items_read != 2) {
    return NULL;
  }

  return &config;
}

int main(int argc, char *argv[]) {
  char *version = "v0.0.0";

  if (argc == 2 && strcmp(argv[1], "-v") == 0) {
    printf("%s\n", version);
    return 0;
  }

  if (argc == 2 && strcmp(argv[1], "init") == 0) {
    return api_geo();
  }

  struct config_t *config = read_config();
  if (config == NULL) {
    printf("Failed to read config\nRun `clotemp init` to initialize your "
           "location\n");
    return 1;
  }

  char *response = api_request(config->latitude, config->longitude);
  if (response) {
    printf("%s\n", response);
    free(response);
  }

  free(config->latitude);
  free(config->longitude);
  return 0;
}