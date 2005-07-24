// San Andreas Audio Extractor - Extracts radio tracks from Grand Theft Auto - San Andreas.
// Copyright (C) 2005 Karl-Johan Karlsson <san-andreas@creideiki.user.lysator.liu.se>

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

// $Id$
// $HeadURL$

// Ogg Vorbis comment manipulation routines adapted from the
// vorbiscomment program (which is GPL) in the vorbis-tools distribution.
// (c) 2000-2002 Michael Smith <msmith@xiph.org>
// (c) 2001 Ralph Giles <giles@ashlu.bc.ca>

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

#include <cstdio>
#include <cmath>
#include <cstring>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/md5.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "config.h"
#include "vcedit.h"

using namespace std;

/* Exit codes:
 * 128 - command line error - usage information printed
 * 1   - error accessing target directory
 * 2   - error accessing source files
 * 4   - error writing output files
 * 8   - sanity check of file contents failed
 * 16  - error reading metadata config file
 * 32  - error adding metadata to output file
 */

//These structs are for demuxing the Ogg streams.
struct ogg_entry
{
  uint32_t size;
  uint32_t foo;
};

struct ogg_header
{
  uint8_t unused1[8000];
  ogg_entry e[8];
  uint8_t unused2[4];
};

void read_header(const uint8_t *source, ogg_header *dest)
{
   //This copy might seem unnecessary, but since we don't know the
   //alignment of the Ogg header in the data stream, we don't know if
   //we can read it immediately.
   memcpy(dest, source, sizeof(ogg_header));
}

struct Memory_File
{
   uint8_t *base;
   size_t size;
   size_t offset;
};

size_t vcedit_read_from_memory(void *dest, size_t chunk_size, size_t num_chunks, void *source)
{
   Memory_File *mem = (Memory_File *)source;
   size_t read_size = min(num_chunks * chunk_size, mem->size - mem->offset);
   memcpy(dest, mem->base + mem->offset, read_size);
   mem->offset += read_size;
   return read_size;
}

void print_usage()
{
   cerr << "Usage:\n"
        << "  extract <file1> [<file2> ...] <target_dir> <metadata_file>\n\n"

        << "Extracts Ogg Vorbis audio files from the Grand Theft Auto -\n"
        << "San Andreas data files <file1>, <file2>, ... and stores the\n"
        << "resulting files in directory <target_dir>, adding metadata\n"
        << "according to <metadata_file>..\n\n" << endl;
}

int main(int argc, char **argv)
{
   if(argc < 4)
   {
      print_usage();
      exit(128);
   }

   ifstream metadata_file(argv[argc - 1], ios::in | ios::binary);
   if(!metadata_file)
   {
      cerr << "Error reading metadata file." << endl;
      exit(16);
   }
   try
   {
      metadata_file >> conf;
   }
   catch(const runtime_error &e)
   {
      cerr << "Error reading metadata file: " << e.what() << endl;
      exit(16);
   }
   metadata_file.close();

   struct stat stat_buffer;
   int result;
   result = stat(argv[argc - 2], &stat_buffer);
   if(result == -1)
   {
      cerr << "Error accessing target directory: " << strerror(errno) << endl;
      exit(1);
   }
   if(!S_ISDIR(stat_buffer.st_mode))
   {
      cerr << "Error accessing target directory: Not a directory." << endl;
      exit(1);
   }

   for(int infile = 1; infile < argc - 2; ++infile)
   {
      string infilename = argv[infile];
      string::size_type last_slash = infilename.rfind('/');
      if(last_slash == string::npos)
      {
         last_slash = 0;
      }
      else
      {
         ++last_slash;
      }
      string inbasename = infilename.substr(last_slash);

      cout << "Processing file " << setw((int)ceil(log10(argc - 3))) << infile
           << " of " << argc - 3 << ": " << infilename << endl;

      result = stat(argv[infile], &stat_buffer);
      if(result == -1)
      {
         cerr << "Error accessing input file " << argv[infile] << ": "
              << strerror(errno) << endl;
         exit(2);
      }
      int file_length = stat_buffer.st_size;

      int in_fd = open(argv[infile], O_RDONLY);
      if(in_fd == -1)
      {
         cerr << "Error opening input file " << argv[infile] << ": "
              << strerror(errno) << endl;
         exit(2);
      }

      //Read the entire file into memory. The largest file is only
      //128MB, which should fit in memory without problems on a
      //machine that can run GTA:SA. Note that the buffer is aligned
      //for a uint32_t, which will help us get more speed later. This
      //bit-fiddling will work as long as an uint8_t does not have
      //more stringent alignment requirements than an uint32_t. I know
      //of no processor that does anything else.
      cout << "   Loading..." << flush;
      uint32_t *file_image = new uint32_t[(file_length / 4) + 1];

      ssize_t read_size = 0;
      while(read_size < file_length)
      {
         result = read(in_fd, file_image + read_size, file_length - read_size);
         if(result == -1 && errno == EINTR)
            continue;
         if(result == -1)
         {
            cerr << "Error reading input file " << argv[infile] << ": "
                 << strerror(errno) << endl;
            exit(2);
         }
         if(result == 0)
         {
            cerr << "Unexpected end of input file " << argv[infile]
                 << " after reading " << read_size
                 << " bytes." << endl;
            exit(2);
         }
         read_size += result;
      }

      close(in_fd);

      cout << " done." << endl;

      cout << "   Decrypting..." << flush;

      //XOR with a constant to break Rockstar's advanced encryption.
      uint32_t magic_number[] = {0xA1C43AEA,
                                 0xF314A89A,
                                 0x23D7B048,
                                 0xF1FFE89D};

      int decryption_state = 0;
      for(int i = 0; i < (file_length / 4) + 1; ++i)
      {
         file_image[i] ^= magic_number[decryption_state];
         decryption_state = (decryption_state + 1) % 4;
      }

      cout << " done." << endl;

      //Fingerprint this file to find the metadata to use
      const unsigned char *md5 = MD5((uint8_t *)file_image, file_length, NULL);
      ostringstream md5hex;
      md5hex << hex << setfill('0');
      for(int i = 0; i < 16; ++i)
         md5hex << setw(2) << (uint32_t)md5[i];
      string md5string = md5hex.str();

      bool add_metadata = true;

      string station = conf.lookup(md5string + ".station", "");
      if(station == "")
      {
         cerr << "   Didn't recognize MD5 hash \"" << md5string
              << "\" for file " << argv[infile]
              << " - not adding metadata." << endl;
         add_metadata = false;
      }
      else
      {
         cout << "   Adding metadata for station " << station << "." << endl;

         if(mkdir((string(argv[argc - 2]) + "/" + station).c_str(), 0755) == -1)
         {
            cerr << "Error creating directory for station " << station
                 << ": " << strerror(errno) << endl;
            exit(32);
         }
      }
      string albumprefix = conf.lookup("global.albumprefix", "");
      if(albumprefix != "")
         albumprefix += " ";

      //Read and demux the Ogg stream.
      int track_num = 1;
      uint8_t *current = (uint8_t *)file_image;
      cout << "   Splitting tracks:" << flush;
      while(current < (uint8_t *)file_image + file_length)
      {
         cout << " " << track_num << flush;
         ogg_header h;
         read_header(current, &h);
         current += sizeof(ogg_header);

         ogg_entry e;
         int i;

         for(i = 0; i < 8; ++i)
         {
            //cout << "Entry: " << i << endl;
            e = h.e[i];
            if(e.size != 0xFFFFFFFF &&
               e.size != 0xCDCDCDCD &&
               e.foo != 0xCDCDCDCD)
               break;
         }

         if(i >= 8)
            continue;

         //Sanity check
         if(string((char *)current, 4) != "OggS")
         {
            cerr << "This doesn't look like an Ogg Vorbis stream to me.\n"
                 << "Expected first 4 bytes to be \"0x"
                 << hex
                 << (unsigned int)'O' << (unsigned int)'g'
                 << (unsigned int)'g' << (unsigned int)'S'
                 << "\", found \"0x"
                 << (unsigned int)current[0] << (unsigned int)current[1]
                 << (unsigned int)current[2] << (unsigned int)current[3]
                 << dec << "\"." << endl;
            exit(8);
         }

         if(add_metadata)
         {
            vcedit_state *state = vcedit_new_state();
            vorbis_comment *vc;

            Memory_File buffer;
            buffer.base = current;
            buffer.size = e.size;
            buffer.offset = 0;
            if(vcedit_open_callbacks(state, &buffer,
                                     &vcedit_read_from_memory,
                                     (vcedit_write_func)fwrite) < 0)
            {
               cerr << "Error initializing vorbid comment edit library while "
                    << "adding metadata for "
                    << argv[infile] << " track " << track_num << ": "
                    << vcedit_error(state) << endl;
               exit(32);
            }

            vc = vcedit_comments(state);
            vorbis_comment_clear(vc);
            vorbis_comment_init(vc);

            vorbis_comment_add_tag(vc, "ALBUM",
                                   const_cast<char *>((albumprefix + station).c_str()));

            ostringstream track_number;
            track_number << track_num;
            vorbis_comment_add_tag(vc, "TRACKNUMBER",
                                   const_cast<char *>(track_number.str().c_str()));

            ostringstream default_title;
            default_title << "Track " << setw(3) << setfill('0') << track_num;
            string title = conf.lookup(md5string + ".track" + track_number.str() + ".title",
                                       default_title.str());
            vorbis_comment_add_tag(vc, "TITLE",
                                   const_cast<char *>(title.c_str()));

            string artist = conf.lookup(md5string + ".track" + track_number.str() + ".artist",
                                        "Rockstar North");
            vorbis_comment_add_tag(vc, "ARTIST",
                                   const_cast<char *>(artist.c_str()));

            FILE *taggedfile = fopen((string(argv[argc - 2]) + "/" +
                                      station + "/" + title + ".ogg").c_str(),
                                     "wb");
            if(!taggedfile)
            {
               cerr << "Error creating tagged file for " << argv[infile]
                    << " track " << track_num << ": "
                    << strerror(errno) << endl;
               exit(32);
            }

            if(vcedit_write(state, taggedfile) < 0)
            {
               cerr << "Error writing tagged file for " << argv[infile]
                    << " track " << track_num << ": "
                    << vcedit_error(state) << endl;
               exit(32);
            }

            if(fclose(taggedfile) == -1)
            {
               cerr << "Error closing output file for " << argv[infile]
                    << " track " << track_num << ": "
                    << strerror(errno) << endl;
               exit(32);
            }

            current += e.size;

            vorbis_comment_clear(vc);
            vcedit_clear(state);
         }
         else
         {
            //Not adding metadata
            ostringstream outfilename;
            outfilename << argv[argc - 2] << "/"
                        << inbasename
                        << "-"
                        << setfill('0')
                        << setw(3) << track_num
                        << ".ogg";
            
            int out_fd = open(outfilename.str().c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
            if(out_fd == -1)
            {
               cerr << "Error opening output file for " << argv[infile] << " track "
                    << track_num << ": " << strerror(errno) << endl;
               exit(4);
            }
            
            unsigned int written_size = 0;
            int result;
            while(written_size < e.size)
            {
               result = write(out_fd, current, e.size - written_size);
               if(result == -1 && errno == EINTR)
                  continue;
               if(result == -1)
               {
                  cerr << "Error writing to output file for " << argv[infile] << " track "
                       << track_num << ": " << strerror(errno) << endl;
                  exit(4);
               }
               written_size += result;
               current += result;
            }

            close(out_fd);
         }

         ++track_num;
      }

      delete[] file_image;
      cout << " done." << endl;
   }
}
