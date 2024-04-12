// SPDX-License-Identifier: GPL-2.0
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "atm90e26.h"

static bool verbose = false;

int main(int argc, char **argv)
{
   int         c;
   const char *name;
   uint32_t    speed = 15625U;

   while ((c = getopt(argc, argv, "hs:v")) != EOF)
   {
      switch (c)
      {
         case 's':
            speed = atoi(optarg);
            if ((speed < 1900) || (speed > 16000000))
            {
               goto usage;
            }

            continue;

         case 'v':
            verbose = true;
            continue;

         case 'h':
         case '?':
usage:
            fprintf(stderr,
               "usage: %s [-h] [-v] [-s Hz] /dev/spidevB.D\n",
               argv[0]);
            return 1;
      }
   }

   if ((optind + 1) != argc)
   {
      goto usage;
   }

   name = argv[optind];

   int32_t status = atm90e26_init(name, speed);
   if (0 != status)
   {
      fprintf(stderr, "Error: %d\n", status);
      perror("atm90e26_init");
      return -1;
   }

   status = atm90e26_start(lgain_24, 20.0F);
   if (0 != status)
   {
      perror("atm90e26_start");
      return -1;
   }

   float irms;
   status = atm90e26_irms_get(&irms);
   if (0 != status)
   {
      perror("atm90e26_irms_get");
      return -1;
   }

   float vrms;
   status = atm90e26_vrms_get(&vrms);
   if (0 != status)
   {
      perror("atm90e26_irms_get");
      return -1;
   }

   float power;
   status = atm90e26_power_get(&power);
   if (0 != status)
   {
      perror("atm90e26_irms_get");
      return -1;
   }

   printf("I (A RMS), V (V RMS), P (W): %g, %g, %g\n", irms, vrms, power);

   status = atm90e26_deinit();
   if (0 != status)
   {
      perror("atm90e26_deinit");
      return -1;
   }

   return 0;
}
