// monitor_sender.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define SERVER_PORT 5000

int get_gateway_ip(char *ip_buffer, size_t size)
{
    FILE *fp = popen("ip route | grep default | awk '{print $3}'", "r");
    if (!fp)
        return -1;
    if (fgets(ip_buffer, size, fp) == NULL)
    {
        pclose(fp);
        return -1;
    }
    ip_buffer[strcspn(ip_buffer, "\n")] = 0;
    pclose(fp);
    return 0;
}

int read_cpu_line(const char *label, unsigned long *total, unsigned long *idle)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp)
        return -1;
    char line[512];
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, label, strlen(label)) == 0)
        {
            unsigned long user, nice, system, idle_t, iowait, irq, softirq, steal;
            if (sscanf(line + strlen(label),
                       "%lu %lu %lu %lu %lu %lu %lu %lu",
                       &user, &nice, &system, &idle_t, &iowait,
                       &irq, &softirq, &steal) < 8)
            {
                fclose(fp);
                return -1;
            }
            *idle = idle_t + iowait;
            *total = user + nice + system + *idle + irq + softirq + steal;
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}

int read_ram(unsigned long *total_kb, unsigned long *available_kb)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp)
        return -1;
    char key[64];
    unsigned long value;
    char unit[16];
    while (fscanf(fp, "%63s %lu %15s\n", key, &value, unit) == 3)
    {
        if (strcmp(key, "MemTotal:") == 0)
            *total_kb = value;
        else if (strcmp(key, "MemAvailable:") == 0)
        {
            *available_kb = value;
            break;
        }
    }
    fclose(fp);
    return 0;
}

int read_temperature(float *temp_c)
{
    FILE *fp = fopen("/sys/devices/soc0/axi/f8007100.adc/iio:device0/in_temp0_raw", "r");
    if (fp)
    {
        int raw;
        if (fscanf(fp, "%d", &raw) == 1)
        {
            fclose(fp);
            *temp_c = ((raw / 4096.0f) * 503.975f) - 273.15f;
            return 0;
        }
        fclose(fp);
    }

    // fallback
    fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp)
    {
        int raw;
        if (fscanf(fp, "%d", &raw) == 1)
        {
            fclose(fp);
            *temp_c = raw / 1000.0f;
            return 0;
        }
        fclose(fp);
    }

    return -1;
}

float get_freq_mhz(int core)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", core);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;
    float freq;
    if (fscanf(fp, "%f", &freq) == 1)
    {
        fclose(fp);
        return freq / 1000.0f;
    }
    fclose(fp);
    return -1;
}

float calculate_cpu_usage(unsigned long prev_total, unsigned long prev_idle,
                          unsigned long curr_total, unsigned long curr_idle)
{
    unsigned long total_diff = curr_total - prev_total;
    unsigned long idle_diff = curr_idle - prev_idle;
    return (total_diff > 0) ? 100.0f * (1.0f - ((float)idle_diff / total_diff)) : 0.0f;
}

int main(int argc, char *argv[])
{
    unsigned int interval_us = 500000; // Default: 500ms

    if (argc > 1)
    {
        int val = atoi(argv[1]);
        if (val > 0)
            interval_us = (unsigned int)val;
    }

    char server_ip[64];
    if (get_gateway_ip(server_ip, sizeof(server_ip)) != 0)
        return 1;

    int sock;
    struct sockaddr_in server_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return 1;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
        return 1;

    while (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed, retrying...");
        sleep(2);
    }

    char interval_msg[64];
    snprintf(interval_msg, sizeof(interval_msg), "INTERVAL_US:%u\n", interval_us);
    send(sock, interval_msg, strlen(interval_msg), 0);

    unsigned long prev_total = 0, prev_idle = 0;
    unsigned long prev0_total = 0, prev0_idle = 0;
    unsigned long prev1_total = 0, prev1_idle = 0;
    usleep(100000);

    // Initialisation unique ici
    unsigned long mem_total_kb = 0, mem_available_kb = 0;

    while (1)
    {
        unsigned long total, idle, total0, idle0, total1, idle1;
        float temp_c;

        read_cpu_line("cpu ", &total, &idle);
        read_cpu_line("cpu0", &total0, &idle0);
        read_cpu_line("cpu1", &total1, &idle1);

        float cpu_usage = calculate_cpu_usage(prev_total, prev_idle, total, idle);
        float cpu0_usage = calculate_cpu_usage(prev0_total, prev0_idle, total0, idle0);
        float cpu1_usage = calculate_cpu_usage(prev1_total, prev1_idle, total1, idle1);

        prev_total = total;
        prev_idle = idle;
        prev0_total = total0;
        prev0_idle = idle0;
        prev1_total = total1;
        prev1_idle = idle1;

        float freq0 = get_freq_mhz(0);
        float freq1 = get_freq_mhz(1);

        if (read_ram(&mem_total_kb, &mem_available_kb) == 0 &&
            read_temperature(&temp_c) == 0)
        {
            float ram_percent = 100.0f * (1.0f - (float)mem_available_kb / mem_total_kb);
            unsigned long ram_used = mem_total_kb - mem_available_kb;

            char message[256];
            snprintf(message, sizeof(message),
                     "CPU:%.2f%%, CPU0:%.2f%%, CPU1:%.2f%%, RAM:%.2f%% (%lu/%lu MB), FREQ0:%.0fMHz, FREQ1:%.0fMHz, Temp:%.2f°C\n",
                     cpu_usage, cpu0_usage, cpu1_usage,
                     ram_percent, ram_used / 1024, mem_total_kb / 1024,
                     freq0, freq1, temp_c);

            send(sock, message, strlen(message), 0);
            printf("Sent: %s", message);
        }

        usleep(interval_us);
    }

    close(sock);
    return 0;
}
