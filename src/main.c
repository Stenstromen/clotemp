#include "../lib/cJSON.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

struct Memory {
  char *response;
  size_t size;
};

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

int main(int argc, char *argv[]) {
  char *version = "v0.0.0";
  char *latitude = "60.17116";
  char *longitude = "24.93265";

  if (strlen(latitude) > MAX_COORD_LEN || strlen(longitude) > MAX_COORD_LEN) {
    fprintf(stderr, "Coordinates exceed max length of %d characters\n",
            MAX_COORD_LEN);
    return 1;
  }

  const char *url_fmt = "https://api.open-meteo.com/v1/"
                        "forecast?latitude=%s&longitude=%s&current="
                        "temperature_2m&timezone=auto";
  const int url_len = snprintf(NULL, 0, url_fmt, latitude, longitude);
  char *url = malloc(url_len + 1);
  sprintf(url, url_fmt, latitude, longitude);

  printf("URL: %s\n", url);

  if (argc == 2 && strcmp(argv[1], "-v") == 0) {
    printf("%s\n", version);
    return 0;
  }

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
            printf("%s // %.1f°C\n", time->valuestring,
                   temperature->valuedouble);
          }
        }
      }
    }
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();
  free(url);

  return 0;
}