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

#ifndef CONFIG_H
#define CONFIG_H

#include <iostream>
#include <map>
#include <stdexcept>

/**
 * This is thrown, with the failed directive as the message, when a
 * directive lookup fails.
 */
class directive_not_found : public std::runtime_error
{
public:
   explicit directive_not_found(const std::string&  __arg) :
      runtime_error(__arg)
   {
   }
};

/**
 * An instance of this class holds all configuration options read from
 * a configuration file.
 */
class config
{
public:

   /**
    * Read a config file into this config object from an istream.
    * \param is The istream to read from.
    * \param conf The config object to populate.
    * \return The istream.
    */
   friend std::istream &operator>>(std::istream &is, config &conf);

   /**
    * Gets the setting(s) of a key.
    * \param key The key of the setting to lookup. This key is
    * of the form 'section.key'.
    * \return The value of the setting.
    * \exception directive_not_found The key was not found.
    */
   std::string operator[](const std::string &key) const;
  
   /**
    * Fetches the value for a key, or a default value if the key does
    * not exist.
    *
    * \param key The key of the setting to lookup. This key is
    * of the form 'section.key'.
    * \param def The default value. Returned if the key was not found.
    * \return The key's value or the default value.
    */
   std::string lookup(const std::string &ns, const std::string &key, const std::string &def) const;

private:

   /**
    * The settings mapping from 'section.key' to 'value'.
    */
   std::map<std::string, std::string> settings;
};

/**
 * The global configuration object.
 */
extern config conf;

#endif
