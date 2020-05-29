// Copyright (C) 2005 Marcus Eriksson <marerk@lysator.liu.se>
//  
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

#include <stdexcept>
#include <sstream>

#include "config.h"

using namespace std;

istream &operator>>(istream &is, config &conf)
{
   string section;
   string line;
   size_t lineno = 0;

   while(getline(is, line))
   {
      lineno++;

      size_t idx = line.find_first_not_of(" \t");
      if(idx == string::npos)
         continue;
      line.erase(0, idx);
      idx = line.find_last_not_of(" \t");
      line.erase(idx + 1);

      switch(line[0])
      {
         case '#': // Comments
         {
            continue;
         }
         break;
         
         case '[':
         {
            idx = line.find(']');
            if(idx == string::npos)
            {
               ostringstream oss;
               oss << "Config file error at line " << lineno << ".";
               throw runtime_error(oss.str());
            }
            else
            {
               section = line.substr(1, idx - 1);
               section.erase(0, section.find_first_not_of(" \t"));
               if(section.size() == 0)
               {
                  ostringstream oss;
                  oss << "Config file error at line " << lineno << ".";
                  throw runtime_error(oss.str());
               }
               else
               {
                  section.erase(section.find_last_not_of(" \t") + 1);
               }
            }
            // look for an alias assignment
            line.erase(0, idx + 1);
            line.erase(0, line.find_first_not_of(" \t"));
            if(line[0] == '=') {
               line.erase(0, 1);
               line.erase(0, line.find_first_not_of(" \t"));
               string parent = line.substr(line.find('['), line.find(']'));
               conf.settings[section + ".parent"] = parent.substr(1, -1);
            }
         }
         break;

         default:
         {
            idx = line.find('=');
            if(idx == string::npos || idx == 0)
            {
               ostringstream oss;
               oss << "Config file error at line " << lineno << ".";
               throw runtime_error(oss.str());
            }
            else if(section.size() == 0)
            {
               throw runtime_error("No section defined before first setting.");
            }
            else
            {
               string key = line.substr(0, idx);
               string val = line.substr(idx + 1);
               key.erase(key.find_last_not_of(" \t") + 1);
               val.erase(0, val.find_first_not_of(" \t"));
               // Associate section.key with val
               conf.settings[section + "." + key] = val;
            }
         }
         break;
      }
   }
   
   return is;
}

string config::operator[](const string &key) const
{
   map<string, string>::const_iterator it = settings.find(key);

   if(it == settings.end())
      throw directive_not_found(key);
   else
      return it->second;
}

string config::lookup(const string &ns, const string &key, const string &def) const
{
   try
   {
      return (*this)[ns + "." + key];
   }
   catch(const directive_not_found &e)
   {
   }
   try
   {
      return this->lookup((*this)[ns + ".parent"], key, def);
   }
   catch(const directive_not_found &e)
   {
      return def;
   }
}

/**
 * The global configuration object.
 */
config conf;
