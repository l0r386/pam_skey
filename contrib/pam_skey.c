/* 
 * (c) 2001 Dinko Korunic, kreator@fly.srk.fer.hr
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *
 * S/KEY is a trademark of Bellcore.
 * Mink is the former name of the S/KEY authentication system.
 *
 * Programs that had some influence in development of this source:
 *  Wietse Venema's logdaemon package
 *  Olaf Kirch's Linux S/Key package
 *  Linux-PAM modules and templates
 *  Wyman Miles' pam_securid module
 *
 * Should you choose to use and/or modify this source code, please do so
 * under the terms of the GNU General Public License under which this
 * program is distributed.
 *
 * NOTE:
 * This is a bit rewritten pam_skey module somewhat applied patch from
 * Norbert Preining <preining@logic.at>. In fact, that patch caused some
 * parts of code to be obsolete, so I rewrote or even purged them -kre.
 */

static char rcsid[] = "$Id$";

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef STRING_WITH_STRINGS
# include <strings.h>
#endif
#include <unistd.h>
#include <pwd.h> 
#include <sys/types.h>
#include <syslog.h>

#define PAM_EXTERN extern
#undef PAM_STATIC

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include "skey.h"
#include "pam_skey.h"
#include "misc.h"

PAM_EXTERN int pam_sm_setcred (pam_handle_t *pamh, int flags,
  int argc, const char **argv)
{
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags,
  int argc, const char **argv)
{
  char challenge[CHALLENGE_MAXSIZE]; /* challenge to print in conv */
  char msg_text[PAM_MAX_MSG_SIZE]; /* text for pam conv */
  char *username = NULL; /* username spacer */
  char *response = NULL; /* response spacer */
  struct skey skey; /* structure that contains skey information */
  int status; /* return status spacer */
  int passwdallowed; /* is password allowed */
  unsigned mod_opt=_MOD_NONE_ON; /* module options */
  char *host; /* points to host */
  char *port; /* points to port */
  struct passwd *pwuser; /* structure for getpw() */

  /* Get module options */
  mod_getopt(&mod_opt, argc, argv);

  /* Get username */
#ifdef LINUX
  if (pam_get_user(pamh, (const char **)&username, "login:")
#else
  if (pam_get_user(pamh, (char **)&username, "login:")
#endif
      != PAM_SUCCESS)
  {
    fprintf(stderr, "cannot determine username\n");
    if (mod_opt & _MOD_DEBUG)
      syslog(LOG_DEBUG, "cannot determine username");
    return PAM_USER_UNKNOWN;
  }

  if (mod_opt & _MOD_DEBUG)
    syslog(LOG_DEBUG, "got username %s", username);

  /* Get host.. */
#ifdef LINUX
  if (pam_get_item(pamh, PAM_RHOST, (const void **)&host)
#else
  if (pam_get_item(pamh, PAM_RHOST, (void **)&host)
#endif
    != PAM_SUCCESS)
      host = NULL; /* couldn't get host */
  /* ..and port */
#ifdef LINUX
  if (pam_get_item(pamh, PAM_TTY, (const void **)&port)
#else
  if (pam_get_item(pamh, PAM_TTY, (void **)&port)
#endif
    != PAM_SUCCESS)
      port = NULL; /* couldn't get port */

  /* Get information from passwd file */
  if ((pwuser = getpwnam(username)) == NULL)
  {
    fprintf(stderr, "no such user\n");
    syslog(LOG_NOTICE, "cannot find user %s", username);
    return PAM_USER_UNKNOWN; /* perhaps even return PAM_ABORT here? */
  }

  /* Do actual checking - we assume skeyaccess() returns PERMIT which is
   * by default 1. Notice 4th argument is NULL - we will not perform
   * address checks on host itself */
  passwdallowed = skeyaccess(pwuser, port, host, NULL);

  if ((mod_opt & _MOD_ACCESS_CHECK) && (passwdallowed != 1))
  {
	  fprintf(stderr, "no s/key access permissions\n");
    syslog(LOG_NOTICE, "no s/key access permissions for %s",
      username);
    return PAM_AUTH_ERR;
  }

  /* Get S/Key information on user with skeyinfo() */
  switch (skeyinfo(&skey, username, NULL))
  {
  /* 0: OK */
  case 0:
    break;
  /* -1: File error */
  case -1:
#if 0
  /* XXX- This seems broken in (at least) logdaemon-5.8. It returns -1
   * when user not found in keyfile. -kre */
    fprintf(stderr, "s/key database error\n");
    syslog(LOG_NOTICE, "s/key database error");
    return PAM_AUTHINFO_UNAVAIL;
#endif
  /* 1: No such user in database */
  case 1:
    /* We won't confuse the ordinary user telling him about missing skeys
     * -kre */
#if 0
    fprintf(stderr, "no s/key for %s\n", username);
#endif
    if (mod_opt & _MOD_DEBUG)
      syslog(LOG_DEBUG, "no s/key for %s\n", username);
    return PAM_AUTHINFO_UNAVAIL;
  }

  /* Make challenge string */
  snprintf(challenge, CHALLENGE_MAXSIZE, "s/key %d %s %s",
      skey.n - 1, skey.seed, passwdallowed ? "allowed" : "required");

  if (mod_opt & _MOD_DEBUG)
    syslog(LOG_DEBUG, "got challenge %s for %s", challenge,
        username);

  /* Read response from last module's PAM_AUTHTOK */
  if (mod_opt & _MOD_USE_FIRST_PASS)
  {
    /* Try to extract authtoken */
#ifdef LINUX
    if (pam_get_item(pamh, PAM_AUTHTOK, (const void **)&response)
#else
    if (pam_get_item(pamh, PAM_AUTHTOK, (void **)&response)
#endif
        != PAM_SUCCESS)
    {
      if (mod_opt & _MOD_DEBUG)
        syslog(LOG_DEBUG, "could not get PAM_AUTHTOK");
      mod_opt &= ~_MOD_USE_FIRST_PASS;
    }
    else
    {
      /* Got AUTHTOK, but it was empty */
      if (empty_authtok(response))
      {
        if (mod_opt & _MOD_DEBUG)
          syslog(LOG_DEBUG, "empty PAM_AUTHTOK");
        mod_opt &= ~_MOD_USE_FIRST_PASS;
      }
      else
        /* All OK, print challenge information */
        fprintf(stderr, "challenge %s\n", challenge);
    }
  }

  /* There was no PAM_AUTHTOK, or there was no such option in pam-conf
   * file */
  if (!(mod_opt & _MOD_USE_FIRST_PASS))
  {
    /* Prepare a complete message for conversation */
    snprintf(msg_text, PAM_MAX_MSG_SIZE,
        "challenge %s\npassword: ", challenge);

    /* Talk with user */
    if (mod_talk_touser(pamh, &mod_opt, msg_text, &response)
        != PAM_SUCCESS)
      return PAM_SERVICE_ERR;

    /* Simulate standard S/Key login procedure - if empty token, turn on
     * ECHO and prompt again */
    if (empty_authtok(response) && !(mod_opt & _MOD_ONLY_ONE_TRY))
    {
      /* Was there echo off? */
      if (mod_opt & _MOD_ECHO_OFF)
      {
        _pam_delete(response);
        fprintf(stderr, "(turning echo on)\n");
        mod_opt &= ~_MOD_ECHO_OFF;

        /* Prepare a complete message for conversation */
        snprintf(msg_text, PAM_MAX_MSG_SIZE, "password: ");

        /* Talk with user */
        if (mod_talk_touser(pamh, &mod_opt, msg_text, &response)
          != PAM_SUCCESS)
          return PAM_SERVICE_ERR;

        /* Got again empty response. Bailout and don't save auth token */
        if (empty_authtok(response))
          return PAM_IGNORE;
      }
      else
      /* There was echo on already - just get out and don't save auth token
       * for other modules */
        return PAM_IGNORE;
    }

    /* XXX - ECHO ON puts '\n' at the end in Solaris 2.7! This is
     * cludge to get rid of this nasty `feature' -kre */
    _pam_degarbage(response);
  
    /* Store auth token - that next module can use with `use_first_pass' */
    if (pam_set_item(pamh, PAM_AUTHTOK, response) != PAM_SUCCESS)
    {
      syslog(LOG_NOTICE, "unable to save auth token");
      return PAM_SERVICE_ERR;
    }
  } 

  /* Verify S/Key */
  status = skeyverify(&skey, response);
  _pam_delete(response);

  switch (status)
  {
    /* 0: Verify successful, database updated */
    case 0:
      break;
    /* -1: Error of some sort; database unchanged */
    /*  1: Verify failed, database unchanged */
    case -1:
    case 1:
      if (mod_opt & _MOD_DEBUG)
        syslog(LOG_DEBUG, "verify for %s failed, database"
            " unchanged", username);
      return PAM_AUTH_ERR;
  }

  /* Success by default */
  return PAM_SUCCESS;
}

/* Get module optional parameters */
static void mod_getopt(unsigned *mod_opt, int mod_argc, const char **mod_argv)
{
  int i;

  /* Setup runtime defaults */
  *mod_opt |= _MOD_DEFAULT_FLAG;
  *mod_opt &= _MOD_DEFAULT_MASK;

  /* Setup runtime options */
  while (mod_argc--)
  {
    for (i = 0; i < _MOD_ARGS; ++i)
    {
      if (mod_args[i].token != NULL &&
          !strncmp(*mod_argv, mod_args[i].token,
            strlen(mod_args[i].token)))
        break;
    }
    if (i >= _MOD_ARGS)
      syslog(LOG_ERR, "unknown option %s", *mod_argv);
    else
    {
      *mod_opt &= mod_args[i].mask; /* Turn off */
      *mod_opt |= mod_args[i].flag; /* Turn on */
    }
    ++mod_argv;
  }
}

/* This will talk to user through PAM_CONV */
static int mod_talk_touser(pam_handle_t *pamh, unsigned *mod_opt,
    char *msg_text, char **response)
{
  struct pam_message message;
  const struct pam_message *pmessage = &message;
  struct pam_conv *conv = NULL;
  struct pam_response *presponse = NULL;

  /* Better safe than sorry */
  *response = NULL;

  /* Be paranoid */
  memset(&message, 0, sizeof(message));

  /* Turn on/off PAM echo */
  if (*mod_opt & _MOD_ECHO_OFF)
    message.msg_style = PAM_PROMPT_ECHO_OFF;
  else
    message.msg_style = PAM_PROMPT_ECHO_ON;
  
  /* Point to conversation text */
  message.msg = msg_text;

  /* Do conversation and see if all is OK */
#ifdef LINUX
  if (pam_get_item(pamh, PAM_CONV, (const void **)&conv)
#else
  if (pam_get_item(pamh, PAM_CONV, (void **)&conv)
#endif
      != PAM_SUCCESS)
  {
    if (*mod_opt & _MOD_DEBUG)
      syslog(LOG_DEBUG, "error in conversation");
    return PAM_SERVICE_ERR;
  }

  /* Convert into pam_response - only 1 reply expected */
#ifdef LINUX
  if (conv->conv(1, &pmessage, &presponse,
        conv->appdata_ptr)
#else
  if (conv->conv(1, (struct pam_message **)&pmessage, &presponse,
        conv->appdata_ptr)
#endif
    != PAM_SUCCESS)
  {
    _pam_delete(presponse->resp);
    return PAM_SERVICE_ERR;
  }

  if (presponse != NULL)
  {
    /* Save address */
    *response = presponse->resp;
    /* To ensure that response address will not be erased */
    presponse->resp = NULL;
    _pam_drop(presponse);
  }
  else
    return PAM_SERVICE_ERR;

  return PAM_SUCCESS;
}
