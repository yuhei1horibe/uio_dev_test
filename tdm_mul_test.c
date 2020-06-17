#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <stdint.h>
#include <time.h>

#define UIO_DEV_PATH "/sys/class/uio/"
#define DEVNAME_MAX 128

static bool find_uio_dev(const char* device_name, char* device_dir, int sz_dev_dir)
{
    DIR*           d;
    struct dirent* dir;
    bool           dev_found = false;
    char dev_dir[DEVNAME_MAX];

    if (!device_name || !device_dir) {
        return false;
    }

	// Find UIO device directory
    d = opendir(UIO_DEV_PATH);
    if (d) {
        int  name_fd    = -1;
        int  bytes_read = 0;
        char name_filename[DEVNAME_MAX];
        char dev_name[DEVNAME_MAX];

		while ((dir = readdir(d)) != NULL) {
            strncpy(dev_dir, UIO_DEV_PATH, DEVNAME_MAX);
            strncat(dev_dir, dir->d_name, DEVNAME_MAX);
            strncpy(name_filename, dev_dir, DEVNAME_MAX);
            strncat(name_filename, "/name", DEVNAME_MAX);

            name_fd = open(name_filename, O_RDONLY);

            // Check "name" file and match the device
            if (name_fd >= 0) {
                bytes_read = read(name_fd, dev_name, DEVNAME_MAX);
                if (bytes_read != 0) {
                    dev_name[bytes_read-1] = '\0';
                    if (strncmp(device_name, dev_name, DEVNAME_MAX) == 0) {
                        strncpy(device_dir, dev_dir, sz_dev_dir);
                        return true;
                    }
                }
                close(name_fd);
            }
		}
		closedir(d);
	}
    return false;
}

static int get_uio_mapping(const char* map_dir, unsigned int* base_addr, unsigned int* mem_size, unsigned int* mem_offset)
{
    char filename[DEVNAME_MAX];
    char buff[DEVNAME_MAX];
    int map_fd = 0;
    int bytes_read = 0;

    if (!base_addr ||
        !mem_size  ||
        !mem_offset) {
        return -1;
    }
    *base_addr  = 0;
    *mem_size   = 0;
    *mem_offset = 0;

    strncpy(filename, map_dir, DEVNAME_MAX);

    // Address
    strncat(filename, "addr", DEVNAME_MAX);
    map_fd = open(filename, O_RDONLY);
    if (map_fd >= 0) {
        bytes_read = read(map_fd, buff, DEVNAME_MAX);
        if (bytes_read > 0) {
            *base_addr = strtoul(buff, NULL, 16);
        }
        close(map_fd);
    }

    // Size
    filename[strlen(map_dir)] = '\0';
    strncat(filename, "size", DEVNAME_MAX);
    map_fd = open(filename, O_RDONLY);
    if (map_fd >= 0) {
        bytes_read = read(map_fd, buff, DEVNAME_MAX);
        if (bytes_read > 0) {
            *mem_size = strtoul(buff, NULL, 16);
        }
        close(map_fd);
    }

    // Offset
    filename[strlen(map_dir)] = '\0';
    strncat(filename, "offset", DEVNAME_MAX);
    map_fd = open(filename, O_RDONLY);
    if (map_fd >= 0) {
        bytes_read = read(map_fd, buff, DEVNAME_MAX);
        if (bytes_read > 0) {
            *mem_offset = strtoul(buff, NULL, 16);
        }
        close(map_fd);
    }
    return 0;
}

// Multiplier test
int mul_test(void* mem_base, unsigned int mem_size)
{
    static const uint32_t num_units = 4;
    uint32_t operand_a[num_units];
    uint32_t operand_b[num_units];
    uint32_t result[num_units];

    uint32_t i;

    srand((unsigned)time(NULL));

    // Generate and write operands
    for (i = 0; i < num_units; i++) {
        operand_a[i] = ((rand() << 16) & 0x7FFF) | rand();
        operand_b[i] = ((rand() << 16) & 0x7FFF) | rand();
        //operand_b[i] = rand() & 0xFF;

        // Calculate correct result
        result[i] = (operand_a[i] * operand_b[i]) >> 8;

        //printf("Writing to: %x and %x\n", (uint32_t)mem_base + sizeof(uint32_t) * 3 * i, (uint32_t)mem_base + sizeof(uint32_t) * 3 * i + sizeof(uint32_t));
        printf("Writing: %x and %x\n", operand_a[i], operand_b[i]);

        break;
    }
    // Write to the register
    *(uint32_t*)(mem_base)                        = operand_a[0];
    *(uint32_t*)(mem_base + sizeof(uint32_t))     = operand_b[0];
    *(uint32_t*)(mem_base + sizeof(uint32_t) * 2) = 4;

    i = 0;
    while (1) {
        if ((*(uint32_t*)(mem_base + sizeof(uint32_t) * 2) & 0x3) != 0) {
            break;
        }
        if (i > (1 << 24)) {
            break;
        }
        i++;
    }

    for (i = 0; i < num_units * 3; i++) {
        printf("Data read: %x (%x)\n", *(uint32_t*)(mem_base + sizeof(uint32_t) * i), (uint32_t)(mem_base + sizeof(uint32_t) * i));
    }
    return 0;
}

// TDM multiplier test
int tdm_mul_test(void* mem_base, unsigned int mem_size)
{
    static const uint32_t num_units = 4;
    uint32_t operand_a[num_units];
    uint32_t operand_b[num_units];
    uint32_t result[num_units];

    uint32_t i;

    // Generate and write operands
    for (i = 0; i < num_units; i++) {
        operand_a[i] = ((rand() << 16) & 0x7F) | rand();
        operand_b[i] = rand() & 0xFF;

        // Calculate correct result
        result[i] = (operand_a[i] * operand_b[i]) >> 8;

        //printf("Writing to: %x and %x\n", (uint32_t)mem_base + sizeof(uint32_t) * 3 * i, (uint32_t)mem_base + sizeof(uint32_t) * 3 * i + sizeof(uint32_t));
        printf("Writing: %x and %x\n", operand_a[i], operand_b[i]);

        // Write to the register
        *(uint32_t*)(mem_base + sizeof(uint32_t) * 3 * i)                    = operand_a[i];
        *(uint32_t*)(mem_base + sizeof(uint32_t) * 3 * i + sizeof(uint32_t)) = operand_b[i];

        //usleep(1000);

        //printf("Reading: %x\n", (uint32_t)(mem_base + sizeof(uint32_t) * 3 * i + sizeof(uint32_t) * 2));
        //printf("Data read: %x\n", *(uint32_t*)(mem_base + sizeof(uint32_t) * 3 * i + sizeof(uint32_t) * 2));

    }
    usleep(500);
    for (i = 0; i < num_units * 3; i++) {
        printf("Data read: %x (%x)\n", *(uint32_t*)(mem_base + sizeof(uint32_t) * i), (uint32_t)(mem_base + sizeof(uint32_t) * i));
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char dev_dir[DEVNAME_MAX];
    char dev_map_dir[DEVNAME_MAX];
    char map_dir[] = "/maps/map0/";
    unsigned int base_addr  = 0;
    unsigned int mem_size   = 0;
    unsigned int mem_offset = 0;

    if (find_uio_dev("zed_uio_module", dev_dir, DEVNAME_MAX) == true) {
        printf("Device found in %s\n", dev_dir);

        strncpy(dev_map_dir, dev_dir, DEVNAME_MAX);
        strncat(dev_map_dir, map_dir, DEVNAME_MAX);
        if (get_uio_mapping(dev_map_dir, &base_addr, &mem_size, &mem_offset) != 0) {
            printf("Failed to get memory mapping from sysfs\n");
            return 0;
        }
        printf("Base address: %x, Memory size: %x, Offset: %u\n", base_addr, mem_size, mem_offset);
        if ((base_addr > 0) && (mem_size > 0)) {
            int uio_fd = 0;
            char uio_filename[DEVNAME_MAX];

            strncpy(uio_filename, "/dev/", DEVNAME_MAX);
            strncat(uio_filename, &dev_dir[strlen(UIO_DEV_PATH)], DEVNAME_MAX);

            // Memory map
            uio_fd = open(uio_filename, O_RDWR);
            if (uio_fd >= 0) {
                printf("UIO device: %s opened: %d\n", uio_filename, uio_fd);
                void* dev_mem_base = mmap((void*)base_addr, mem_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, uio_fd, mem_offset);
                if (dev_mem_base != NULL) {
                    int result = 0;
                    printf("mmap() success\n\n");

                    // Add test here
                    //result = tdm_mul_test(dev_mem_base, mem_size);
                    result = mul_test(dev_mem_base, mem_size);

                    munmap(dev_mem_base, mem_size);
                }
                close(uio_fd);
            }

            else {
                printf("Failed to open device: %s\n", uio_filename);
            }
        }
        else {
            printf("Invalid memory address or size\n");
            return 0;
        }
    }
    return 0;
}
