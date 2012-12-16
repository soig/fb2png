#include <fcntl.h>
#include <png.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int main(int argc, char *argv[])
{
	char *fbdevice = "/dev/fb0";
    int fbfd = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long framebuffersize = 0;
    unsigned char *fbp = NULL;

	char *pngname = "fb.png";
	png_structp png_ptr;
	png_infop info_ptr;
	FILE *pngfp = NULL;
	png_bytep png_buffer = NULL;

    int y = 0;
    long int location = 0;

	int opt;

	//--------------------------------------------------------------------

	while ((opt = getopt(argc, argv, "d:p:")) != -1)
	{
		switch (opt)
		{
		case 'd':

			fbdevice = optarg;
			break;

		case 'p':

			pngname = optarg;
			break;

		default:

			fprintf(stderr, "Usage: %s [-d device] [-p pngname]\n",
					argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	//--------------------------------------------------------------------

    fbfd = open(fbdevice, O_RDWR);
    if (fbfd == -1)
	{
        perror("Error: cannot open framebuffer");
        exit(EXIT_FAILURE);
    }

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1)
	{
        perror("Error: reading framebuffer fixed information");
        exit(EXIT_FAILURE);
    }

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1)
	{
        perror("Error: reading framebuffer variable information");
        exit(EXIT_FAILURE);
    }

    framebuffersize = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8);

    fbp = (unsigned char *)mmap(0,
								framebuffersize,
								PROT_READ | PROT_WRITE,
								MAP_SHARED,
								fbfd,
								0);
    if ((int)fbp == -1)
	{
        perror("Error: failed to map framebuffer device to memory");
        exit(EXIT_FAILURE);
    }

	//--------------------------------------------------------------------

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
									  NULL,
									  NULL,
									  NULL);
	if (png_ptr == NULL)
	{
		fprintf(stderr, "Error: could not allocate PNG write struct\n");
		exit(EXIT_FAILURE);
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		fprintf(stderr, "Error: could not allocate PNG info struct\n");
		exit(EXIT_FAILURE);
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fprintf(stderr, "Error: creating PNG\n");
		exit(EXIT_FAILURE);
	}

	pngfp = fopen(pngname, "wb");

	if (pngfp == NULL)
	{
		fprintf(stderr, "Error: Unable to create %s\n", pngname);
		exit(EXIT_FAILURE);
	}

	png_init_io(png_ptr, pngfp);

	png_set_IHDR(
		png_ptr,
		info_ptr,
		vinfo.xres,
		vinfo.yres,
		8,
		PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE,
		PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	png_buffer = (png_bytep)malloc(vinfo.xres * 3 * sizeof(png_byte));

	if (png_buffer == NULL)
	{
		fprintf(stderr, "Unable to allocate buffer\n");
		exit(0);
	}

	//--------------------------------------------------------------------

    for (y = 0; y < vinfo.yres; y++)
	{
		int x;

        for (x = 0; x < vinfo.xres; x++)
		{
			int pb_offset = 3 * x;

            location = (x+vinfo.xoffset)
					 * (vinfo.bits_per_pixel/8)
					 + (y+vinfo.yoffset)
					 * finfo.line_length;

            if (vinfo.bits_per_pixel == 32)
			{
				png_buffer[pb_offset + 2] = *(fbp + location);
				png_buffer[pb_offset + 1] = *(fbp + location + 1);
				png_buffer[pb_offset] = *(fbp + location + 2);
            }
			else
			{
                unsigned short int t = *((unsigned short int*)(fbp + location));
                int b = (((t >> 11) & 0x1F) * 255) / 31;
                int g = (((t >> 5) & 0x3F) * 255) / 63;
                int r = ((t & 0x1F) * 255) / 31;

				png_buffer[pb_offset + 2] = r;
				png_buffer[pb_offset + 1] = g;
				png_buffer[pb_offset] = b;
            }
        }

		png_write_row(png_ptr, png_buffer);
	}

	//--------------------------------------------------------------------

	free(png_buffer);
	png_buffer = NULL;
	png_write_end(png_ptr, NULL);
	fclose(pngfp);

	//--------------------------------------------------------------------

    munmap(fbp, framebuffersize);
    close(fbfd);

	//--------------------------------------------------------------------

    return 0;
}
