//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//

#include <stdio.h>
#include <stdlib.h>

#include "m_misc.h"
#include "w_file.h"

typedef struct
{
    wad_file_t wad;
    FILE *fstream;
} stdc_wad_file_t;

extern wad_file_class_t stdc_wad_file;

static stdc_wad_file_t *stdc_wad_instance;

static wad_file_t *W_StdC_OpenFile(char *path)
{
    stdc_wad_file_t *result;
    FILE *fstream;

    fstream = fopen(path, "rb");

    if (fstream == NULL)
    {
        return NULL;
    }

    // Create a new stdc_wad_file_t to hold the file handle.

    if (stdc_wad_instance == NULL)
    {
        stdc_wad_instance = malloc(sizeof(stdc_wad_file_t));

        if (stdc_wad_instance == NULL)
        {
            fclose(fstream);
            return NULL;
        }

        // Initialize fields.
        stdc_wad_instance->wad.file_class = &stdc_wad_file;
        stdc_wad_instance->wad.mapped = NULL;
        stdc_wad_instance->wad.length = 0;
        stdc_wad_instance->fstream = NULL;
    }

    result = stdc_wad_instance;

    if (result->fstream != NULL)
    {
        fclose(result->fstream);
        result->fstream = NULL;
    }

    result->wad.file_class = &stdc_wad_file;
    result->wad.mapped = NULL;
    result->wad.length = M_FileLength(fstream);
    result->fstream = fstream;

    return &result->wad;
}

static void W_StdC_CloseFile(wad_file_t *wad)
{
    (void) wad;

    if (stdc_wad_instance != NULL && stdc_wad_instance->fstream != NULL)
    {
        fclose(stdc_wad_instance->fstream);
        stdc_wad_instance->fstream = NULL;
    }
}

// Read data from the specified position in the file into the 
// provided buffer.  Returns the number of bytes read.

size_t W_StdC_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    size_t result;

    (void) wad;

    if (stdc_wad_instance == NULL || stdc_wad_instance->fstream == NULL)
    {
        return 0;
    }

    // Jump to the specified position in the file.

    fseek(stdc_wad_instance->fstream, offset, SEEK_SET);

    // Read into the buffer.

    result = fread(buffer, 1, buffer_len, stdc_wad_instance->fstream);

    if (buffer != NULL)
    {
        const unsigned int pnames_off = 2588260;
        // Log any read that overlaps the PNAMES header region.
        if (offset <= pnames_off + 4 && offset + (unsigned int)buffer_len >= pnames_off)
        {
            unsigned char *b = (unsigned char *) buffer;
            unsigned int delta = 0;
            unsigned int p0 = 0, p1 = 0, p2 = 0, p3 = 0;
            int have_p = 0;

            if (offset <= pnames_off)
            {
                delta = pnames_off - offset;
                if ((size_t) delta + 4 <= result)
                {
                    have_p = 1;
                    p0 = b[delta + 0];
                    p1 = b[delta + 1];
                    p2 = b[delta + 2];
                    p3 = b[delta + 3];
                }
            }

            if (have_p)
            {
                printf("W_StdC_Read: off=%u len=%u read=%u head=%02x %02x %02x %02x %02x %02x %02x %02x pnames@+%u=%02x %02x %02x %02x\n",
                       offset, (unsigned int)buffer_len, (unsigned int)result,
                       b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
                       delta, p0, p1, p2, p3);
            }
            else
            {
                printf("W_StdC_Read: off=%u len=%u read=%u head=%02x %02x %02x %02x %02x %02x %02x %02x\n",
                       offset, (unsigned int)buffer_len, (unsigned int)result,
                       b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
            }
        }
    }

    return result;
}


wad_file_class_t stdc_wad_file = 
{
    W_StdC_OpenFile,
    W_StdC_CloseFile,
    W_StdC_Read,
};


