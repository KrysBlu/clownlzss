/*
	(C) 2018-2020 Clownacy

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
*/

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chameleon.h"
#include "comper.h"
#include "faxman.h"
#include "kosinski.h"
#include "kosinskiplus.h"
#include "rage.h"
#include "rocket.h"
#include "saxman.h"

typedef enum Format
{
	FORMAT_CHAMELEON,
	FORMAT_COMPER,
	FORMAT_KOSINSKI,
	FORMAT_KOSINSKIPLUS,
	FORMAT_RAGE,
	FORMAT_ROCKET,
	FORMAT_SAXMAN,
	FORMAT_SAXMAN_NO_HEADER,
	FORMAT_FAXMAN
} Format;

typedef struct Mode
{
	const char *command;
	Format format;
	const char *normal_default_filename;
	const char *moduled_default_filename;
} Mode;

static const Mode modes[] = {
	{"-ch", FORMAT_CHAMELEON, "out.cham", "out.chamm"},
	{"-c", FORMAT_COMPER, "out.comp", "out.compm"},
	{"-k", FORMAT_KOSINSKI, "out.kos", "out.kosm"},
	{"-kp", FORMAT_KOSINSKIPLUS, "out.kosp", "out.kospm"},
	{"-ra", FORMAT_RAGE, "out.rage", "out.ragem"},
	{"-r", FORMAT_ROCKET, "out.rock", "out.rockm"},
	{"-s", FORMAT_SAXMAN, "out.sax", "out.saxm"},
	{"-sn", FORMAT_SAXMAN_NO_HEADER, "out.sax", "out.saxm"},
	{"-f", FORMAT_FAXMAN, "out.fax", "out.faxm"},
};

static void PrintUsage(void)
{
	printf(
	"Clownacy's compression tool thingy\n"
	"\n"
	"Usage: tool [options] [in-filename] [out-filename]"
	"\n"
	"Options:\n"
	"\n"
	" Format:\n"
	"  -ch    Compress in Chameleon format\n"
	"  -c     Compress in Comper format\n"
	"  -k     Compress in Kosinski format\n"
	"  -kp    Compress in Kosinski+ format\n"
	"  -ra    Compress in Rage format\n"
	"  -r     Compress in Rocket format\n"
	"  -s     Compress in Saxman format\n"
	"  -sn    Compress in Saxman format (with no header)\n"
	"  -f     Compress in Faxman format\n"
	"\n"
	" Misc:\n"
	"  -m[=MODULE_SIZE]  Compresses into modules\n"
	"                    MODULE_SIZE controls the module size (defaults to 0x1000)\n"
	);
}

int main(int argc, char *argv[])
{
	--argc;
	++argv;

	const Mode *mode = NULL;
	const char *in_filename = NULL;
	const char *out_filename = NULL;
	bool moduled = false;
	size_t module_size = 0x1000;

	for (int i = 0; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
			{
				PrintUsage();
			}
			else if (!strncmp(argv[i], "-m", 2))
			{
				moduled = true;

				char *argument = strchr(argv[i], '=');

				if (argument)
				{
					char *end;
					unsigned long result = strtoul(argument + 1, &end, 0);

					if (*end != '\0')
					{
						printf("Invalid parameter to -m\n");
						return -1;
					}
					else
					{
						module_size = result;

						if (module_size > 0x1000)
							printf("Warning: the moduled format header does not fully support sizes greater than\n 0x1000 - header will likely be invalid!");
					}
				}
			}
			else
			{
				for (size_t j = 0; j < sizeof(modes) / sizeof(modes[0]); ++j)
				{
					if (!strcmp(argv[i], modes[j].command))
					{
						mode = &modes[j];
						break;
					}
				}
			}
		}
		else
		{
			if (!in_filename)
				in_filename = argv[i];
			else
				out_filename = argv[i];
		}
	}

	if (!in_filename)
	{
		printf("Error: Input file not specified\n\n");
		PrintUsage();
	}
	else if (!mode)
	{
		printf("Error: Format not specified\n\n");
		PrintUsage();
	}
	else
	{
		if (!out_filename)
			out_filename = moduled ? mode->moduled_default_filename : mode->normal_default_filename;

		FILE *in_file = fopen(in_filename, "rb");

		if (in_file)
		{
			fseek(in_file, 0, SEEK_END);
			const size_t file_size = ftell(in_file);
			rewind(in_file);

			unsigned char *file_buffer = (unsigned char*)malloc(file_size);
			fread(file_buffer, 1, file_size, in_file);
			fclose(in_file);

			size_t compressed_size;
			unsigned char *compressed_buffer = NULL;

			switch (mode->format)
			{
				case FORMAT_CHAMELEON:
					if (moduled)
						compressed_buffer = ClownLZSS_ModuledChameleonCompress(file_buffer, file_size, &compressed_size, module_size);
					else
						compressed_buffer = ClownLZSS_ChameleonCompress(file_buffer, file_size, &compressed_size);
					break;

				case FORMAT_COMPER:
					if (moduled)
						compressed_buffer = ClownLZSS_ModuledComperCompress(file_buffer, file_size, &compressed_size, module_size);
					else
						compressed_buffer = ClownLZSS_ComperCompress(file_buffer, file_size, &compressed_size);
					break;

				case FORMAT_KOSINSKI:
					if (moduled)
						compressed_buffer = ClownLZSS_ModuledKosinskiCompress(file_buffer, file_size, &compressed_size, module_size);
					else
						compressed_buffer = ClownLZSS_KosinskiCompress(file_buffer, file_size, &compressed_size);
					break;

				case FORMAT_KOSINSKIPLUS:
					if (moduled)
						compressed_buffer = ClownLZSS_ModuledKosinskiPlusCompress(file_buffer, file_size, &compressed_size, module_size);
					else
						compressed_buffer = ClownLZSS_KosinskiPlusCompress(file_buffer, file_size, &compressed_size);
					break;

				case FORMAT_RAGE:
					if (moduled)
						compressed_buffer = ClownLZSS_ModuledRageCompress(file_buffer, file_size, &compressed_size, module_size);
					else
						compressed_buffer = ClownLZSS_RageCompress(file_buffer, file_size, &compressed_size);
					break;

				case FORMAT_ROCKET:
					if (moduled)
						compressed_buffer = ClownLZSS_ModuledRocketCompress(file_buffer, file_size, &compressed_size, module_size);
					else
						compressed_buffer = ClownLZSS_RocketCompress(file_buffer, file_size, &compressed_size);
					break;

				case FORMAT_SAXMAN:
					if (moduled)
						compressed_buffer = ClownLZSS_ModuledSaxmanCompress(file_buffer, file_size, &compressed_size, true, module_size);
					else
						compressed_buffer = ClownLZSS_SaxmanCompress(file_buffer, file_size, &compressed_size, true);
					break;

				case FORMAT_SAXMAN_NO_HEADER:
					if (moduled)
						compressed_buffer = ClownLZSS_ModuledSaxmanCompress(file_buffer, file_size, &compressed_size, false, module_size);
					else
						compressed_buffer = ClownLZSS_SaxmanCompress(file_buffer, file_size, &compressed_size, false);
					break;

				case FORMAT_FAXMAN:
					if (moduled)
						compressed_buffer = ClownLZSS_ModuledFaxmanCompress(file_buffer, file_size, &compressed_size, module_size);
					else
						compressed_buffer = ClownLZSS_FaxmanCompress(file_buffer, file_size, &compressed_size);
					break;
			}

			if (compressed_buffer)
			{
				FILE *out_file = fopen(out_filename, "wb");

				if (out_file)
				{
					fwrite(compressed_buffer, compressed_size, 1, out_file);
					free(compressed_buffer);
					fclose(out_file);
				}
			}
		}
	}
}
