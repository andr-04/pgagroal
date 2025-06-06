/*
 * Copyright (C) 2025 The pgagroal community
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may
 * be used to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* pgagroal */
#include <pgagroal.h>
#include <aes.h>
#include <json.h>
#include <logging.h>
#include <management.h>
#include <security.h>
#include <stdint.h>
#include <time.h>
#include <utils.h>

/* system */
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static int master_key(char* password, bool generate_pwd, int pwd_length, int32_t output_format);
static int add_user(char* users_path, char* username, char* password, bool generate_pwd, int pwd_length, int32_t output_format);
static int update_user(char* users_path, char* username, char* password, bool generate_pwd, int pwd_length, int32_t output_format);
static int remove_user(char* users_path, char* username, int32_t output_format);
static int list_users(char* users_path, int32_t output_format);
static int create_response(char* users_path, struct json* json, struct json** response);

const struct pgagroal_command command_table[] =
{
   {
      .command = "master-key",
      .subcommand = "",
      .accepted_argument_count = {0},
      .deprecated = false,
      .action = MANAGEMENT_MASTER_KEY,
      .log_message = "<master-key>",
   },
   {
      .command = "user",
      .subcommand = "add",
      .accepted_argument_count = {0},
      .deprecated = false,
      .action = MANAGEMENT_ADD_USER,
      .log_message = "<user add> [%s]",
   },
   {
      .command = "user",
      .subcommand = "edit",
      .accepted_argument_count = {0},
      .deprecated = false,
      .action = MANAGEMENT_UPDATE_USER,
      .log_message = "<user edit> [%s]",
   },
   {
      .command = "user",
      .subcommand = "del",
      .accepted_argument_count = {0},
      .deprecated = false,
      .action = MANAGEMENT_REMOVE_USER,
      .log_message = "<user del> [%s]",
   },
   {
      .command = "user",
      .subcommand = "ls",
      .accepted_argument_count = {0},
      .deprecated = false,
      .action = MANAGEMENT_LIST_USERS,
      .log_message = "<user ls>",
   },
};

static void
version(void)
{
   printf("pgagroal-admin %s\n", PGAGROAL_VERSION);
   exit(1);
}

static void
usage(void)
{
   printf("pgagroal-admin %s\n", PGAGROAL_VERSION);
   printf("  Administration utility for pgagroal\n");
   printf("\n");

   printf("Usage:\n");
   printf("  pgagroal-admin [ -f FILE ] [ COMMAND ] \n");
   printf("\n");
   printf("Options:\n");
   printf("  -f, --file FILE         Set the path to a user file\n");
   printf("                          Defaults to %s\n", PGAGROAL_DEFAULT_USERS_FILE);
   printf("  -U, --user USER         Set the user name\n");
   printf("  -P, --password PASSWORD Set the password for the user\n");
   printf("  -g, --generate          Generate a password\n");
   printf("  -l, --length            Password length\n");
   printf("  -V, --version           Display version information\n");
   printf("  -F, --format text|json  Set the output format\n");
   printf("  -?, --help              Display help\n");
   printf("\n");
   printf("Commands:\n");
   printf("  master-key              Create or update the master key\n");
   printf("  user <subcommand>       Manage a specific user, where <subcommand> can be\n");
   printf("                          - add  to add a new user\n");
   printf("                          - del  to remove an existing user\n");
   printf("                          - edit to change the password for an existing user\n");
   printf("                          - ls   to list all available users\n");
   printf("\n");
   printf("pgagroal: %s\n", PGAGROAL_HOMEPAGE);
   printf("Report bugs: %s\n", PGAGROAL_ISSUES);
}

int
main(int argc, char** argv)
{
   int c;
   char* username = NULL;
   char* password = NULL;
   char* file_path = NULL;
   bool generate_pwd = false;
   int pwd_length = DEFAULT_PASSWORD_LENGTH;
   int option_index = 0;
   size_t command_count = sizeof(command_table) / sizeof(struct pgagroal_command);
   struct pgagroal_parsed_command parsed = {.cmd = NULL, .args = {0}};
   int32_t output_format = MANAGEMENT_OUTPUT_FORMAT_TEXT;

   while (1)
   {
      static struct option long_options[] =
      {
         {"user", required_argument, 0, 'U'},
         {"password", required_argument, 0, 'P'},
         {"file", required_argument, 0, 'f'},
         {"generate", no_argument, 0, 'g'},
         {"length", required_argument, 0, 'l'},
         {"version", no_argument, 0, 'V'},
         {"format", required_argument, 0, 'F'},
         {"help", no_argument, 0, '?'}
      };

      c = getopt_long(argc, argv, "gV?f:U:P:l:F:",
                      long_options, &option_index);

      if (c == -1)
      {
         break;
      }

      switch (c)
      {
         case 'U':
            username = optarg;
            break;
         case 'P':
            password = optarg;
            break;
         case 'f':
            file_path = optarg;
            break;
         case 'g':
            generate_pwd = true;
            break;
         case 'l':
            pwd_length = atoi(optarg);
            break;
         case 'V':
            version();
            break;
         case 'F':
            if (!strncmp(optarg, "json", MISC_LENGTH))
            {
               output_format = MANAGEMENT_OUTPUT_FORMAT_JSON;
            }
            else if (!strncmp(optarg, "text", MISC_LENGTH))
            {
               output_format = MANAGEMENT_OUTPUT_FORMAT_TEXT;
            }
            else
            {
               warnx("pgagroal-cli: Format type is not correct");
               exit(1);
            }
            break;
         case '?':
            usage();
            exit(1);
            break;
         default:
            break;
      }
   }

   if (getuid() == 0)
   {
      errx(1, "Using the root account is not allowed");
   }

   if (!parse_command(argc, argv, optind, &parsed, command_table, command_count))
   {
      usage();
      goto error;
   }

   // if here, the action is understood, but we need
   // the file to operate onto!
   // Therefore, if the user did not specify any config file
   // the default one is used. Note that in the case of ACTION_MASTER_KEY
   // there is no need for the file_path to be set, so setting to a default
   // value does nothing.
   // Setting the file also means we don't have to check against the file_path value.
   if (file_path == NULL)
   {
      file_path = PGAGROAL_DEFAULT_USERS_FILE;
   }

   if (parsed.cmd->action == MANAGEMENT_MASTER_KEY)
   {
      if (master_key(password, generate_pwd, pwd_length, output_format))
      {
         errx(1, "Cannot generate master key");
      }
   }
   else if (parsed.cmd->action == MANAGEMENT_ADD_USER)
   {
      if (add_user(file_path, username, password, generate_pwd, pwd_length, output_format))
      {
         errx(1, "Error for <user add>");
      }
   }
   else if (parsed.cmd->action == MANAGEMENT_UPDATE_USER)
   {
      if (update_user(file_path, username, password, generate_pwd, pwd_length, output_format))
      {
         errx(1, "Error for <user edit>");
      }
   }
   else if (parsed.cmd->action == MANAGEMENT_REMOVE_USER)
   {

      if (remove_user(file_path, username, output_format))
      {
         errx(1, "Error for <user del>");
      }
   }
   else if (parsed.cmd->action == MANAGEMENT_LIST_USERS)
   {

      if (list_users(file_path, output_format))
      {
         errx(1, "Error for <user ls>");
      }

   }

   exit(0);

error:

   exit(1);
}

static int
master_key(char* password, bool generate_pwd, int pwd_length, int32_t output_format)
{
   FILE* file = NULL;
   char buf[MISC_LENGTH];
   char* encoded = NULL;
   size_t encoded_length;
   struct stat st = {0};
   bool do_free = true;
   struct json* j = NULL;
   struct json* outcome = NULL;
   time_t start_t;
   time_t end_t;

   start_t = time(NULL);

   if (pgagroal_management_create_header(MANAGEMENT_MASTER_KEY, 0, 0, output_format, &j))
   {
      goto error;
   }

   if (password != NULL)
   {
      do_free = false;
   }

   if (pgagroal_get_home_directory() == NULL)
   {
      char* username = pgagroal_get_user_name();

      if (username != NULL)
      {
         warnx("No home directory for user \'%s\'", username);
      }
      else
      {
         warnx("No home directory for user running pgagroal");
      }

      goto error;
   }

   memset(&buf, 0, sizeof(buf));
   snprintf(&buf[0], sizeof(buf), "%s/.pgagroal", pgagroal_get_home_directory());

   if (stat(&buf[0], &st) == -1)
   {
      mkdir(&buf[0], S_IRWXU);
   }
   else
   {
      if (S_ISDIR(st.st_mode) && st.st_mode & S_IRWXU && !(st.st_mode & S_IRWXG) && !(st.st_mode & S_IRWXO))
      {
         /* Ok */
      }
      else
      {
         warnx("Wrong permissions for directory <%s> (must be 0700)", &buf[0]);
         goto error;
      }
   }

   memset(&buf, 0, sizeof(buf));
   snprintf(&buf[0], sizeof(buf), "%s/.pgagroal/master.key", pgagroal_get_home_directory());

   if (pgagroal_exists(&buf[0]))
   {
      warnx("The file %s already exists, cannot continue", &buf[0]);
      goto error;
   }

   if (stat(&buf[0], &st) == -1)
   {
      /* Ok */
   }
   else
   {
      if (S_ISREG(st.st_mode) && st.st_mode & (S_IRUSR | S_IWUSR) && !(st.st_mode & S_IRWXG) && !(st.st_mode & S_IRWXO))
      {
         /* Ok */
      }
      else
      {
         warnx("Wrong permissions for file <%s> (must be 0600)", &buf[0]);
         goto error;
      }
   }

   file = fopen(&buf[0], "w+");
   if (file == NULL)
   {
      warnx("Could not write to master key file <%s>", &buf[0]);
      goto error;
   }

   #if defined(HAVE_OSX)
      #define PGAGROAL_GETENV(name) getenv(name)
   #else
      #define PGAGROAL_GETENV(name) secure_getenv(name)
   #endif

   if (password == NULL)
   {
      if (generate_pwd)
      {
         if (pgagroal_generate_password(pwd_length, &password))
         {
            do_free = false;
            goto error;
         }
      }
      else
      {
         password = PGAGROAL_GETENV("PGAGROAL_PASSWORD");

         if (password == NULL)
         {
            while (password == NULL)
            {
               printf("Master key (will not echo): ");
               password = pgagroal_get_password();
               printf("\n");

               if (password != NULL && strlen(password) < MIN_PASSWORD_LENGTH)
               {
                  printf("Invalid key length, must be at least %d chars.\n", MIN_PASSWORD_LENGTH);
                  free(password);
                  password = NULL;
               }
            }
         }
         else
         {
            do_free = false;
         }
      }
   }
   else
   {
      do_free = false;
   }

   end_t = time(NULL);

   if (pgagroal_management_create_outcome_success(j, start_t, end_t, &outcome))
   {
      goto error;
   }

   if (output_format == MANAGEMENT_OUTPUT_FORMAT_JSON)
   {
      pgagroal_json_print(j, FORMAT_JSON);
   }
   else
   {
      pgagroal_json_print(j, FORMAT_TEXT);
   }

   pgagroal_base64_encode(password, strlen(password), &encoded, &encoded_length);
   fputs(encoded, file);
   free(encoded);

   pgagroal_json_destroy(j);

   if (do_free)
   {
      free(password);
   }

   fclose(file);
   file = NULL;

   chmod(&buf[0], S_IRUSR | S_IWUSR);
   printf("Master Key stored into %s\n", &buf[0]);
   return 0;

error:

   free(encoded);

   if (do_free)
   {
      free(password);
   }

   if (file)
   {
      fclose(file);
      file = NULL;
   }

   pgagroal_management_create_outcome_failure(j, 1, &outcome);

   if (output_format == MANAGEMENT_OUTPUT_FORMAT_JSON)
   {
      pgagroal_json_print(j, FORMAT_JSON);
   }
   else
   {
      pgagroal_json_print(j, FORMAT_TEXT);
   }

   pgagroal_json_destroy(j);

   return 1;
}

static int
add_user(char* users_path, char* username, char* password, bool generate_pwd, int pwd_length, int32_t output_format)
{
   FILE* users_file = NULL;
   char line[MISC_LENGTH];
   char* entry = NULL;
   char* master_key = NULL;
   char* ptr = NULL;
   char* encrypted = NULL;
   int encrypted_length = 0;
   char* encoded = NULL;
   size_t encoded_length;
   char un[MAX_USERNAME_LENGTH];
   int number_of_users = 0;
   bool do_verify = true;
   char* verify = NULL;
   bool do_free = true;
   struct json* j = NULL;
   struct json* outcome = NULL;
   struct json* response = NULL;
   time_t start_t;
   time_t end_t;

   start_t = time(NULL);

   if (pgagroal_management_create_header(MANAGEMENT_ADD_USER, 0, 0, output_format, &j))
   {
      goto error;
   }

   if (pgagroal_get_master_key(&master_key))
   {
      warnx("Invalid master key");
      goto error;
   }

   if (password != NULL)
   {
      do_verify = false;
      do_free = false;
   }

   users_file = fopen(users_path, "a+");
   if (users_file == NULL)
   {
      warnx("Could not append to users file <%s>", users_path);
      goto error;
   }

   /* User */
   if (username == NULL)
   {
username:
      printf("User name: ");

      memset(&un, 0, sizeof(un));
      if (fgets(&un[0], sizeof(un), stdin) == NULL)
      {
         goto error;
      }
      un[strlen(un) - 1] = 0;
      username = &un[0];
   }

   if (username == NULL || strlen(username) == 0)
   {
      goto username;
   }

   /* Verify */
   while (fgets(line, sizeof(line), users_file))
   {
      ptr = strtok(line, ":");
      if (!strcmp(username, ptr))
      {
         warnx("Existing user: %s", username);
         goto error;
      }

      number_of_users++;
   }

   if (number_of_users > NUMBER_OF_USERS)
   {
      warnx("Too many users");
      goto error;
   }

   /* Password */
   if (password == NULL)
   {
password:
      if (generate_pwd)
      {
         if (pgagroal_generate_password(pwd_length, &password))
         {
            do_free = false;
            goto error;
         }
         do_verify = false;
         printf("Password : %s", password);
      }
      else
      {
         password = PGAGROAL_GETENV("PGAGROAL_PASSWORD");

         if (password == NULL)
         {
            printf("Password : ");

            if (password != NULL)
            {
               free(password);
               password = NULL;
            }

            password = pgagroal_get_password();
         }
         else
         {
            do_free = false;
            do_verify = false;
         }
      }
      printf("\n");
   }

   for (unsigned long i = 0; i < strlen(password); i++)
   {
      if ((unsigned char)(*(password + i)) & 0x80)
      {
         goto password;
      }
   }

   if (do_verify)
   {
      printf("Verify   : ");

      if (verify != NULL)
      {
         free(verify);
         verify = NULL;
      }

      verify = pgagroal_get_password();
      printf("\n");

      if (strlen(password) != strlen(verify) || memcmp(password, verify, strlen(password)) != 0)
      {
         goto password;
      }
   }

   pgagroal_encrypt(password, master_key, &encrypted, &encrypted_length, ENCRYPTION_AES_256_CBC);
   pgagroal_base64_encode(encrypted, encrypted_length, &encoded, &encoded_length);

   entry = pgagroal_append(entry, username);
   entry = pgagroal_append(entry, ":");
   entry = pgagroal_append(entry, encoded);
   entry = pgagroal_append(entry, "\n");

   fputs(entry, users_file);

   free(entry);
   free(master_key);
   free(encrypted);
   free(encoded);
   if (do_free)
   {
      free(password);
   }
   free(verify);

   fclose(users_file);
   users_file = NULL;

   end_t = time(NULL);

   if (pgagroal_management_create_outcome_success(j, start_t, end_t, &outcome))
   {
      goto error;
   }

   if (create_response(users_path, j, &response))
   {
      goto error;
   }

   if (output_format == MANAGEMENT_OUTPUT_FORMAT_JSON)
   {
      pgagroal_json_print(j, FORMAT_JSON);
   }
   else
   {
      pgagroal_json_print(j, FORMAT_TEXT);
   }

   pgagroal_json_destroy(j);

   return 0;

error:

   free(entry);
   free(master_key);
   free(encrypted);
   free(encoded);
   if (do_free)
   {
      free(password);
   }
   free(verify);

   if (users_file)
   {
      fclose(users_file);
      users_file = NULL;
   }

   pgagroal_management_create_outcome_failure(j, 1, &outcome);

   if (output_format == MANAGEMENT_OUTPUT_FORMAT_JSON)
   {
      pgagroal_json_print(j, FORMAT_JSON);
   }
   else
   {
      pgagroal_json_print(j, FORMAT_TEXT);
   }

   pgagroal_json_destroy(j);

   return 1;
}

static int
update_user(char* users_path, char* username, char* password, bool generate_pwd, int pwd_length, int32_t output_format)
{
   FILE* users_file = NULL;
   FILE* users_file_tmp = NULL;
   char tmpfilename[MISC_LENGTH];
   char line[MISC_LENGTH];
   char line_copy[MISC_LENGTH];
   char* master_key = NULL;
   char* ptr = NULL;
   char* entry = NULL;
   char* encrypted = NULL;
   int encrypted_length = 0;
   char* encoded = NULL;
   size_t encoded_length;
   char un[MAX_USERNAME_LENGTH];
   bool found = false;
   bool do_verify = true;
   char* verify = NULL;
   bool do_free = true;
   struct json* j = NULL;
   struct json* outcome = NULL;
   struct json* response = NULL;
   time_t start_t;
   time_t end_t;

   start_t = time(NULL);

   if (pgagroal_management_create_header(MANAGEMENT_UPDATE_USER, 0, 0, output_format, &j))
   {
      goto error;
   }

   memset(&tmpfilename, 0, sizeof(tmpfilename));

   if (pgagroal_get_master_key(&master_key))
   {
      warnx("Invalid master key");
      goto error;
   }

   if (password != NULL)
   {
      do_verify = false;
      do_free = false;
   }

   users_file = fopen(users_path, "r");
   if (!users_file)
   {
      warnx("File <%s> not found", users_path);
      goto error;
   }

   snprintf(tmpfilename, sizeof(tmpfilename), "%s.tmp", users_path);
   users_file_tmp = fopen(tmpfilename, "w+");
   if (users_file_tmp == NULL)
   {
      warnx("Could not write to temporary user file <%s>", tmpfilename);
      goto error;
   }

   /* User */
   if (username == NULL)
   {
username:
      printf("User name: ");

      memset(&un, 0, sizeof(un));
      if (fgets(&un[0], sizeof(un), stdin) == NULL)
      {
         goto error;
      }
      un[strlen(un) - 1] = 0;
      username = &un[0];
   }

   if (username == NULL || strlen(username) == 0)
   {
      goto username;
   }

   /* Update */
   while (fgets(line, sizeof(line), users_file))
   {
      memset(&line_copy, 0, sizeof(line_copy));
      memcpy(&line_copy, &line, strlen(line));

      ptr = strtok(line, ":");
      if (!strcmp(username, ptr))
      {
         /* Password */
         if (password == NULL)
         {
password:
            if (generate_pwd)
            {
               if (pgagroal_generate_password(pwd_length, &password))
               {
                  do_free = false;
                  goto error;
               }
               do_verify = false;
               printf("Password : %s", password);
            }
            else
            {
               password = PGAGROAL_GETENV("PGAGROAL_PASSWORD");

               if (password == NULL)
               {
                  printf("Password : ");

                  if (password != NULL)
                  {
                     free(password);
                     password = NULL;
                  }

                  password = pgagroal_get_password();
               }
               else
               {
                  do_free = false;
                  do_verify = false;
               }
            }
            printf("\n");
         }

         for (unsigned long i = 0; i < strlen(password); i++)
         {
            if ((unsigned char)(*(password + i)) & 0x80)
            {
               goto password;
            }
         }

         if (do_verify)
         {
            printf("Verify   : ");

            if (verify != NULL)
            {
               free(verify);
               verify = NULL;
            }

            verify = pgagroal_get_password();
            printf("\n");

            if (strlen(password) != strlen(verify) || memcmp(password, verify, strlen(password)) != 0)
            {
               goto password;
            }
         }

         pgagroal_encrypt(password, master_key, &encrypted, &encrypted_length, ENCRYPTION_AES_256_CBC);
         pgagroal_base64_encode(encrypted, encrypted_length, &encoded, &encoded_length);

         entry = NULL;
         entry = pgagroal_append(entry, username);
         entry = pgagroal_append(entry, ":");
         entry = pgagroal_append(entry, encoded);
         entry = pgagroal_append(entry, "\n");

         fputs(entry, users_file_tmp);
         free(entry);

         found = true;
      }
      else
      {
         fputs(line_copy, users_file_tmp);
      }
   }

   if (!found)
   {
      warnx("User '%s' not found", username);
      goto error;
   }

   free(master_key);
   free(encrypted);
   free(encoded);
   if (do_free)
   {
      free(password);
   }
   free(verify);

   fclose(users_file);
   users_file = NULL;
   fclose(users_file_tmp);
   users_file_tmp = NULL;

   rename(tmpfilename, users_path);

   end_t = time(NULL);

   if (pgagroal_management_create_outcome_success(j, start_t, end_t, &outcome))
   {
      goto error;
   }

   if (create_response(users_path, j, &response))
   {
      goto error;
   }

   if (output_format == MANAGEMENT_OUTPUT_FORMAT_JSON)
   {
      pgagroal_json_print(j, FORMAT_JSON);
   }
   else
   {
      pgagroal_json_print(j, FORMAT_TEXT);
   }

   pgagroal_json_destroy(j);

   return 0;

error:

   free(master_key);
   free(encrypted);
   free(encoded);
   if (do_free)
   {
      free(password);
   }
   free(verify);

   if (users_file)
   {
      fclose(users_file);
      users_file = NULL;
   }

   if (users_file_tmp)
   {
      fclose(users_file_tmp);
      users_file_tmp = NULL;
   }

   if (strlen(tmpfilename) > 0)
   {
      remove(tmpfilename);
   }

   pgagroal_management_create_outcome_failure(j, 1, &outcome);

   if (output_format == MANAGEMENT_OUTPUT_FORMAT_JSON)
   {
      pgagroal_json_print(j, FORMAT_JSON);
   }
   else
   {
      pgagroal_json_print(j, FORMAT_TEXT);
   }

   pgagroal_json_destroy(j);

   return 1;
}

static int
remove_user(char* users_path, char* username, int32_t output_format)
{
   FILE* users_file = NULL;
   FILE* users_file_tmp = NULL;
   char tmpfilename[MISC_LENGTH];
   char line[MISC_LENGTH];
   char line_copy[MISC_LENGTH];
   char* ptr = NULL;
   char un[MAX_USERNAME_LENGTH];
   bool found = false;
   struct json* j = NULL;
   struct json* outcome = NULL;
   struct json* response = NULL;
   time_t start_t;
   time_t end_t;

   start_t = time(NULL);

   if (pgagroal_management_create_header(MANAGEMENT_REMOVE_USER, 0, 0, output_format, &j))
   {
      goto error;
   }

   users_file = fopen(users_path, "r");
   if (!users_file)
   {
      warnx("File <%s> not found", users_path);
      goto error;
   }

   memset(&tmpfilename, 0, sizeof(tmpfilename));
   snprintf(tmpfilename, sizeof(tmpfilename), "%s.tmp", users_path);
   users_file_tmp = fopen(tmpfilename, "w+");
   if (users_file_tmp == NULL)
   {
      warnx("Could not write to temporary user file <%s>", tmpfilename);
      goto error;
   }

   /* User */
   if (username == NULL)
   {
username:
      printf("User name: ");

      memset(&un, 0, sizeof(un));
      if (fgets(&un[0], sizeof(un), stdin) == NULL)
      {
         goto error;
      }
      un[strlen(un) - 1] = 0;
      username = &un[0];
   }

   if (username == NULL || strlen(username) == 0)
   {
      goto username;
   }

   /* Remove */
   while (fgets(line, sizeof(line), users_file))
   {
      memset(&line_copy, 0, sizeof(line_copy));
      memcpy(&line_copy, &line, strlen(line));

      ptr = strtok(line, ":");
      if (!strcmp(username, ptr))
      {
         found = true;
      }
      else
      {
         fputs(line_copy, users_file_tmp);
      }
   }

   if (!found)
   {
      warnx("User '%s' not found", username);
      goto error;
   }

   fclose(users_file);
   users_file = NULL;
   fclose(users_file_tmp);
   users_file_tmp = NULL;

   rename(tmpfilename, users_path);

   end_t = time(NULL);

   if (pgagroal_management_create_outcome_success(j, start_t, end_t, &outcome))
   {
      goto error;
   }

   if (create_response(users_path, j, &response))
   {
      goto error;
   }

   if (output_format == MANAGEMENT_OUTPUT_FORMAT_JSON)
   {
      pgagroal_json_print(j, FORMAT_JSON);
   }
   else
   {
      pgagroal_json_print(j, FORMAT_TEXT);
   }

   pgagroal_json_destroy(j);

   return 0;

error:

   if (users_file)
   {
      fclose(users_file);
      users_file = NULL;
   }

   if (users_file_tmp)
   {
      fclose(users_file_tmp);
      users_file_tmp = NULL;
   }

   if (strlen(tmpfilename) > 0)
   {
      remove(tmpfilename);
   }

   pgagroal_management_create_outcome_failure(j, 1, &outcome);

   if (output_format == MANAGEMENT_OUTPUT_FORMAT_JSON)
   {
      pgagroal_json_print(j, FORMAT_JSON);
   }
   else
   {
      pgagroal_json_print(j, FORMAT_TEXT);
   }

   pgagroal_json_destroy(j);

   return 1;
}

static int
list_users(char* users_path, int32_t output_format)
{
   FILE* users_file = NULL;
   char line[MISC_LENGTH];
   char* ptr = NULL;
   struct json* j = NULL;
   struct json* outcome = NULL;
   struct json* response = NULL;
   time_t start_t;
   time_t end_t;

   start_t = time(NULL);

   if (pgagroal_management_create_header(MANAGEMENT_LIST_USERS, 0, 0, output_format, &j))
   {
      goto error;
   }

   users_file = fopen(users_path, "r");
   if (!users_file)
   {
      goto error;
   }

   /* List */
   while (fgets(line, sizeof(line), users_file))
   {
      ptr = strtok(line, ":");
      if (strchr(ptr, '\n'))
      {
         continue;
      }
      printf("%s\n", ptr);
   }

   fclose(users_file);
   users_file = NULL;
   users_file = NULL;

   end_t = time(NULL);

   if (pgagroal_management_create_outcome_success(j, start_t, end_t, &outcome))
   {
      goto error;
   }

   if (create_response(users_path, j, &response))
   {
      goto error;
   }

   if (output_format == MANAGEMENT_OUTPUT_FORMAT_JSON)
   {
      pgagroal_json_print(j, FORMAT_JSON);
   }
   else
   {
      pgagroal_json_print(j, FORMAT_TEXT);
   }

   pgagroal_json_destroy(j);

   return 0;

error:

   if (users_file)
   {
      fclose(users_file);
      users_file = NULL;
   }

   return 1;
}

static int
create_response(char* users_path, struct json* json, struct json** response)
{
   struct json* r = NULL;
   struct json* users = NULL;
   FILE* users_file = NULL;
   char line[MISC_LENGTH];
   char* ptr = NULL;

   *response = NULL;

   if (pgagroal_json_create(&r))
   {
      goto error;
   }

   pgagroal_json_put(json, MANAGEMENT_CATEGORY_RESPONSE, (uintptr_t)r, ValueJSON);

   if (pgagroal_json_create(&users))
   {
      goto error;
   }

   users_file = fopen(users_path, "r");
   if (!users_file)
   {
      goto error;
   }

   while (fgets(line, sizeof(line), users_file))
   {
      ptr = strtok(line, ":");
      if (strchr(ptr, '\n'))
      {
         continue;
      }
      pgagroal_json_append(users, (uintptr_t)ptr, ValueString);
   }

   pgagroal_json_put(r, "Users", (uintptr_t)users, ValueJSON);

   *response = r;

   return 0;

error:

   pgagroal_json_destroy(r);

   return 1;
}
