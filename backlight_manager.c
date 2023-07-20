/*
 * backlight_manager - A program to manage backlight settings.
 *
 * Copyright (C) 2023 Adrian Danzglock <caedael@posteo.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PATH_LENGTH 512

// Function to get the path of the brightness sensor
char* get_sensor_path(const char* devices_path, const char* filename) {
  DIR* dir = opendir(devices_path);
  if (dir == NULL) {
    perror("Error opening iio devices directory");
    return NULL;
  }

  struct dirent* entry;
  char path[MAX_PATH_LENGTH];
  while ((entry = readdir(dir)) != NULL) {

    // Skip '.' and '..' directories
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Create the full path to the current entry
    snprintf(path, MAX_PATH_LENGTH, "%s/%s/%s", devices_path, entry->d_name, filename);

    // Check if the file exists at the current path
    FILE* fp = fopen(path, "r");
    if (fp != NULL) {
      // File found, close the file and return the path
      fclose(fp);
      closedir(dir);
      snprintf(path, MAX_PATH_LENGTH, "%s/%s", devices_path, entry->d_name);
      char* result = strdup(path);
      return result;
    }
  }

  closedir(dir);
  return NULL; // No sensor with the required file found
}

// Function to construct the full path of the config file
// It uses the XDG_CONFIG_HOME environment variable if available, otherwise falls back to the default location
const char* get_config_file_path() {
  const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home != NULL) {
    static char config_file_path[512];
    snprintf(config_file_path, sizeof(config_file_path), "%s/backlight_manager/backlight_manager.conf", xdg_config_home);
    return config_file_path;
  } else {
    return ".config/backlight_manager/backlight_manager.conf";
  }
}

// Structure to store configuration data
typedef struct {
  char sensor_path[256];
  char sensor_file[256];
  char sensor_file_path[256];
  char keyboard_backlight_path[256];
  char screen_backlight_path[256];
  double brightness_factor;
  int update_rate;
} ConfigData;

// Function to read configuration data from the config file
ConfigData read_config_data() {
  ConfigData config;
  FILE* fp = fopen(get_config_file_path(), "r");
  if (fp != NULL) {
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
      // Remove newline character from the line
      line[strcspn(line, "\n")] = '\0';

      char key[256], value[256];
      if (sscanf(line, "%255[^=]=%255s", key, value) == 2) {
        if (strcmp(key, "sensor_path") == 0) {
          strncpy(config.sensor_path, value, sizeof(config.sensor_path));
        } else if (strcmp(key, "sensor_file") == 0) {
          strncpy(config.sensor_file, value, sizeof(config.sensor_file));
        } else if (strcmp(key, "keyboard_backlight_path") == 0) {
          strncpy(config.keyboard_backlight_path, value, sizeof(config.keyboard_backlight_path));
        } else if (strcmp(key, "screen_backlight_path") == 0) {
          strncpy(config.screen_backlight_path, value, sizeof(config.screen_backlight_path));
        } else if (strcmp(key, "update_rate") == 0) {
          config.update_rate = atoi(value);
        } else if (strcmp(key, "brightness_factor") == 0) {
          sscanf(value, "%lf", &config.brightness_factor);
        }
      }
    }
    fclose(fp);
  } else {
    perror("could not open config file");
  }
  char* sensor_file_path = get_sensor_path(config.sensor_path, config.sensor_file);
  if (sensor_file_path == NULL) {
    perror("Sensor file not found");
    exit(EXIT_FAILURE);
  }
  strncpy(config.sensor_file_path, sensor_file_path, sizeof(config.sensor_file_path));
  free(sensor_file_path);
  return config;
}

// Function to read a file
int read_file(const char* file_path, const char* filename) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", file_path, filename);
  FILE* fp = fopen(path, "r");
  if (fp == NULL) {
    printf("%s\n", path);
    perror("Error reading file");
    exit(EXIT_FAILURE);
  }

  int brightness;
  fscanf(fp, "%d", &brightness);
  fclose(fp);
  return brightness;
}

// Function to set the backlight brightness
void set_backlight_brightness(const char* backlight_path, int brightness, int max_brightness) {
  if (brightness < 1) {
    brightness = 1;
  } else if (brightness > max_brightness) {
    brightness = max_brightness;
  }
  char path[512];
  snprintf(path, sizeof(path), "%s/brightness", backlight_path);
  FILE* fp = fopen(path, "w");
  if (fp == NULL) {
    perror("Error write to brightness file");
    exit(EXIT_FAILURE);
  }
  fprintf(fp, "%d", brightness);
  fclose(fp);
}

// Function to display usage information
void display_usage() {
  printf("Usage: backlight_manager [OPTIONS]\n");
  printf("Options:\n");
  printf("  -h, --help             Display this help and exit\n");
  printf("  -a, --ambient          Enable ambient mode\n");
  printf("  -p, --print-status     Print the actual status of the daemon\n");
  printf("  -s, --set <value>      Set change of brightness\n");
}

// Function to print the actual config values
void print_info(const ConfigData* config) {
  printf("Backlight Manager Config:\n");
  printf("  Sensor Path: %s\n", config->sensor_path);
  printf("  Sensor File: %s\n", config->sensor_file);
  printf("  Sensor File Path: %s\n", config->sensor_file_path);
  printf("  Keyboard Backlight Path: %s\n", config->keyboard_backlight_path);
  printf("  Screen Backlight Path: %s\n", config->screen_backlight_path);
  printf("  Update Rate: %d\n", config->update_rate);
  printf("  Brightness Factor: %f\n", config->brightness_factor);
}

int main(int argc, char* argv[]) {
  int ambient_mode = 0; // Default value: ambient mode disabled
  int brightness_adjustment = 0; // Default value: no brightness adjustment
  bool print_status = false; // Default value: do not print status
  ConfigData config = read_config_data();

  // Parse command-line options using getopt

  int option;
  const char* short_options = "hapeds:k:b:";
  static struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"ambient", no_argument, NULL, 'a'},
    {"print-status", no_argument, NULL, 'p'},
    {"set", required_argument, NULL, 's'},
    {NULL, 0, NULL, 0}
  };

  while ((option = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
    switch (option) {
      case 'h':
        display_usage();
        return 0;
      case 'a':
        ambient_mode = 1;
        break;
      case 'p':
        print_status = true;
        break;
      case 's':
        brightness_adjustment = atoi(optarg);
        break;
      default:
        fprintf(stderr, "Unknown option: %c\n", option);
        return 1;
    }
  }

  if (print_status) {
    print_info(&config);
    return 0;
  }

  int max_screen_brightness = read_file(config.screen_backlight_path, "max_brightness");

  if (brightness_adjustment != 0) {
    int current_screen_brightness = read_file(config.screen_backlight_path, "actual_brightness");
    set_backlight_brightness(config.screen_backlight_path, current_screen_brightness + (int)((max_screen_brightness / 100.0) * brightness_adjustment), max_screen_brightness);
  }


  while (ambient_mode) {
    double illumination = read_file(config.sensor_file_path, config.sensor_file);

    int backlight_value = (int)(illumination * config.brightness_factor);

    set_backlight_brightness(config.screen_backlight_path, backlight_value, max_screen_brightness);

    sleep(config.update_rate);
  }

  return 0;
}
