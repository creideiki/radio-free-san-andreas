// San Andreas Audio Extractor - Extracts radio tracks from Grand Theft Auto - San Andreas.
// Copyright (C) 2005 Karl-Johan Karlsson <creideiki+san-andreas@ferretporn.se>

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

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdio>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

using namespace std;

/* Exit codes:
 * 128 - command line error - usage information printed
 * 1   - error accessing target directory
 * 2   - error accessing source files
 * 4   - error writing output files
 * 8   - sanity check of file contents failed
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
   memcpy(dest, source, sizeof(ogg_header));
}

void print_usage()
{
   cerr << "Usage:\n"
        << "  extract [-v] <file1> [<file2> ...] <target_dir>\n\n"

        << "Extracts Ogg Vorbis audio files from the Grand Theft Auto -\n"
        << "San Andreas data files <file1>, <file2>, ... and stores the\n"
        << "resulting files in directory <target_dir>.\n\n" << endl;
}

int main(int argc, char **argv)
{
   if(argc < 3)
   {
      print_usage();
      exit(128);
   }

   struct stat stat_buffer;
   int result;
   result = stat(argv[argc - 1], &stat_buffer);
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

   for(int infile = 1; infile < argc - 1; ++infile)
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

      cout << "Processing file " << setw((argc - 2) / 10 + 1) << infile
           << " of " << argc - 2 << ": " << infilename << endl;

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
      result = read(in_fd, file_image, file_length);
      if(result == -1)
      {
         cerr << "Error reading input file " << argv[infile] << ": "
              << strerror(errno) << endl;
         exit(2);
      }
      else if(result != file_length)
      {
         cerr << "Short read count from input file " << argv[infile] << ": "
              << strerror(errno) << endl;
         exit(2);
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
      for(uint32_t i = 0; i < (file_length / 4) + 1; ++i)
      {
         file_image[i] ^= magic_number[decryption_state];
         decryption_state = (decryption_state + 1) % 4;
      }

      cout << " done." << endl;

      //Read and demux the Ogg stream.
      int num_tracks = 1;
      int num_bytes = 0;
      uint8_t *current = (uint8_t *)file_image;
      cout << "   Splitting tracks:" << flush;
      while(current < (uint8_t *)file_image + file_length)
      {
         cout << " " << num_tracks << flush;
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
            cerr << "\nThis doesn't look like an Ogg Vorbis stream to me.\n"
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

         ostringstream outfilename;
         outfilename << argv[argc - 1] << "/"
                     << inbasename
                     << "-"
                     << setfill('0')
                     << setw(3) << num_tracks
                     << ".ogg";

         int out_fd = creat(outfilename.str().c_str(), 0644);
         if(out_fd == -1)
         {
            cerr << "Error opening output file for " << argv[infile] << " track "
                 << num_tracks << ": " << strerror(errno) << endl;
            exit(4);
         }

         int written_size = 0;
         int result;
         while(written_size < e.size)
         {
            result = write(out_fd, current, e.size - written_size);
            if(result == -1)
            {
               cerr << "Error writing to output file for " << argv[infile] << " track "
                    << num_tracks << ": " << strerror(errno) << endl;
               exit(4);
            }
            written_size += result;
            current += result;
         }
         close(out_fd);
         ++num_tracks;
      }
      cout << " done." << endl;
   }
}
